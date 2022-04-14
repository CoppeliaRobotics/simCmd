#include "qluacommanderwidget.h"
#include "UIProxy.h"
#include "UIFunctions.h"
#include <boost/format.hpp>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QTimer>
#include <QToolTip>
#include <QApplication>
#include <QLabel>

#ifdef Q_OS_MACOS
#define Q_REAL_CTRL Qt::MetaModifier
#else
#define Q_REAL_CTRL Qt::ControlModifier
#endif

QGlobalEventFilter * QGlobalEventFilter::instance_ = NULL;

QGlobalEventFilter::QGlobalEventFilter(QLuaCommanderWidget *commander, QLuaCommanderEdit *widget)
    : commander_(commander), widget_(widget)
{
}

bool QGlobalEventFilter::eventFilter(QObject *object, QEvent *event)
{
    if(event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if(keyEvent->key() == Qt::Key_C && keyEvent->modifiers().testFlag(Q_REAL_CTRL) && keyEvent->modifiers().testFlag(Qt::AltModifier))
        {
            if(widget_->isVisible())
                widget_->setFocus();
            return true;
        }
    }
    return QObject::eventFilter(object, event);
}

void QGlobalEventFilter::install(QLuaCommanderWidget *commander, QLuaCommanderEdit *widget)
{
    instance_ = new QGlobalEventFilter(commander, widget);
    QCoreApplication::instance()->installEventFilter(instance_);
}

void QGlobalEventFilter::uninstall()
{
    QCoreApplication::instance()->removeEventFilter(instance_);
    instance_ = nullptr;
}

static inline bool isID(QChar c)
{
    return c.isLetterOrNumber() || c == '_' || c == '.';
}

bool tokenBehindCursor(const QString &cmd, int cursorPos, QString *tok, QChar *ctx, int *pos)
{
    QString before = cmd.left(cursorPos);
    QString after = cmd.mid(cursorPos);

    // will not complete if in the middle of a symbol
    if(!after.isEmpty() && isID(after[0]))
        return false;

    int j = before.length() - 1;
    while(j >= 0 && isID(before[j])) j--;
    *pos = ++j;
    *tok = before.mid(j);
    *ctx = 'i'; // identifier

    // check if we are inside a string...
    QChar prev{0}, inStr{0};
    int strStart = -1;
    for(int i = 0; i < before.length(); i++)
    {
        QChar c{before[i]};
        if(inStr != 0 && prev == '\\') goto breakChecks;
        if(inStr != 0 && c == inStr) inStr = 0;
        if(inStr == 0 && (c == '\'' || c == '"')) {inStr = c; strStart = i;}
breakChecks:
        prev = c;
    }

    if(inStr != 0 && strStart > 0 && before[strStart - 1] == 'H')
    {
        *pos = strStart + 1;
        *tok = before.mid(strStart + 1);
        *ctx = 'H';
    }

    return true;
}

QLuaCommanderEdit::QLuaCommanderEdit(QLuaCommanderWidget *parent)
    : QCommandEdit(parent),
      commander(parent)
{
    QGlobalEventFilter::install(parent, this);
}

QLuaCommanderEdit::~QLuaCommanderEdit()
{
    QGlobalEventFilter::uninstall();
}

void QLuaCommanderEdit::keyPressEvent(QKeyEvent *event)
{
    auto w = static_cast<QLuaCommanderWidget*>(parent());

    if(event->key() == Qt::Key_ParenLeft)
    {
        acceptCompletion();

#ifndef USE_LUA_PARSER
        emit askCallTip(text(), cursorPosition());
#endif // USE_LUA_PARSER
    }
    else if(event->key() == Qt::Key_Period)
    {
        acceptCompletion();
    }
    else if(event->key() == Qt::Key_Comma)
    {
#ifndef USE_LUA_PARSER
        // reshow last tooltip when comma is pressed
        w->onSetCallTip(toolTip());
#endif // USE_LUA_PARSER
    }
    else if(event->key() == Qt::Key_ParenRight)
    {
#ifndef USE_LUA_PARSER
        w->onSetCallTip("");
#endif // USE_LUA_PARSER
    }
    else if(event->key() == Qt::Key_L && event->modifiers().testFlag(Q_REAL_CTRL))
    {
        emit clearConsole();
        return;
    }
    QCommandEdit::keyPressEvent(event);
}

QLuaCommanderWidget::QLuaCommanderWidget(QWidget *parent)
    : QWidget(parent)
{
    statusbarSize = UIProxy::getInstance()->getStatusbarSize();
    const int k = 100;
    statusbarSizeFocused.push_back(statusbarSize[0] - k);
    statusbarSizeFocused.push_back(statusbarSize[1] + k);

    closeFlag.store(false);

    QGridLayout *grid = new QGridLayout(this);
    grid->setSpacing(0);
    grid->setMargin(0);
    setLayout(grid);

#ifdef CUSTOM_TOOLTIP_FOR_CALLTIPS
    calltipLabel = new QLabel(this);
    calltipLabel->setVisible(false);
#endif // CUSTOM_TOOLTIP_FOR_CALLTIPS
    editor = new QLuaCommanderEdit(this);
    editor->setPlaceholderText("Input Lua code here, or type \"help()\" (use TAB for auto-completion)");
    editor->setFont(QFont("Courier", 12));
    scriptCombo = new QComboBox(this);
    scriptCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    closeButton = new QPushButton(this);
    closeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    closeButton->setFlat(true);
    closeButton->setStyleSheet("margin-left: 5px; margin-right: 5px; font-size: 14pt;");

#ifdef CUSTOM_TOOLTIP_FOR_CALLTIPS
    grid->addWidget(calltipLabel, 0, 0);
#endif // CUSTOM_TOOLTIP_FOR_CALLTIPS
    grid->addWidget(editor, 1, 0);
    grid->addWidget(scriptCombo, 1, 1);
    grid->addWidget(closeButton, 1, 2);

    connect(editor, &QLuaCommanderEdit::askCompletion, this, &QLuaCommanderWidget::onAskCompletion);
    connect(editor, &QLuaCommanderEdit::askCallTip, this, &QLuaCommanderWidget::onAskCallTip);
    connect(editor, &QLuaCommanderEdit::execute, this, &QLuaCommanderWidget::onExecute);
    connect(editor, &QLuaCommanderEdit::escape, this, &QLuaCommanderWidget::onEscape);
    connect(editor, &QLuaCommanderEdit::editorCleared, this, &QLuaCommanderWidget::onEditorCleared);
    connect(editor, &QLuaCommanderEdit::textChanged, this, &QLuaCommanderWidget::onEditorChanged);
    connect(editor, &QLuaCommanderEdit::cursorPositionChanged, this, &QLuaCommanderWidget::onEditorCursorChanged);
    connect(editor, &QLuaCommanderEdit::clearConsole, this, &QLuaCommanderWidget::onClearConsole);
    connect(closeButton, &QPushButton::clicked, this, &QLuaCommanderWidget::onClose);
    connect(QApplication::instance(), SIGNAL(focusChanged(QWidget*,QWidget*)), this, SLOT(onGlobalFocusChanged(QWidget*,QWidget*)));
}

QLuaCommanderWidget::~QLuaCommanderWidget()
{
}

void QLuaCommanderWidget::setOptions(const PersistentOptions &options_)
{
    options = options_;
    editor->setShowMatchingHistory(options.showMatchingHistory);
}

bool QLuaCommanderWidget::getSelectedScriptInfo(int &type, int &handle, QString &name)
{
    if(scriptCombo->currentIndex() >= 0)
    {
        QVariantList data = scriptCombo->itemData(scriptCombo->currentIndex()).toList();
        type = data[0].toInt();
        handle = data[1].toInt();
        name = data[2].toString();
        return true;
    }
    else return false;
}

void QLuaCommanderWidget::onAskCompletion(const QString &cmd, int cursorPos)
{
    int type = sim_scripttype_sandboxscript;
    int handle = -1;
    QString name;
    getSelectedScriptInfo(type, handle, name);

    QString token;
    QChar context;
    int startPos;
    if(tokenBehindCursor(cmd, cursorPos, &token, &context, &startPos))
    {
        sim::addLog(sim_verbosity_debug, "cmd=\"%s\", pos=%d, tbc=%s, ctx=%s", cmd.toStdString(), cursorPos, token.toStdString(), context.toLatin1());

        emit askCompletion(type, name, token, context);
    }
}

void QLuaCommanderWidget::onAskCallTip(QString input, int pos)
{
    int type = sim_scripttype_sandboxscript;
    int handle = -1;
    QString name;
    getSelectedScriptInfo(type, handle, name);
    emit askCallTip(type, input, pos);
}

void QLuaCommanderWidget::onExecute(const QString &cmd)
{
    int type = sim_scripttype_sandboxscript;
    int handle = -1;
    QString name;
    getSelectedScriptInfo(type, handle, name);
    emit execCode(cmd, type, name);
    editor->clear();
    onSetCallTip("");
}

void QLuaCommanderWidget::onEscape()
{
    editor->clearFocus();
}

void QLuaCommanderWidget::onEditorCleared()
{
    onSetCallTip("");
}

void QLuaCommanderWidget::onEditorChanged(QString text)
{
    onAskCallTip(text, editor->cursorPosition());
}

void QLuaCommanderWidget::onEditorCursorChanged(int oldPos, int newPos)
{
    onAskCallTip(editor->text(), newPos);
}

void QLuaCommanderWidget::onClose()
{
    closeFlag.store(true);
}

void QLuaCommanderWidget::onClearConsole()
{
    simAddStatusbarMessage(NULL);
}

void QLuaCommanderWidget::onFocusIn()
{
    if(options.resizeStatusbarWhenFocused)
        expandStatusbar();
}

void QLuaCommanderWidget::onFocusOut()
{
    if(options.resizeStatusbarWhenFocused)
        contractStatusbar();
}

void QLuaCommanderWidget::expandStatusbar()
{
    if(!statusbarExpanded)
    {
        statusbarSize = UIProxy::getInstance()->getStatusbarSize();
        UIProxy::getInstance()->setStatusbarSize(statusbarSizeFocused);
        statusbarExpanded = true;
    }
}

void QLuaCommanderWidget::contractStatusbar()
{
    if(statusbarExpanded)
    {
        statusbarSizeFocused = UIProxy::getInstance()->getStatusbarSize();
        UIProxy::getInstance()->setStatusbarSize(statusbarSize);
        statusbarExpanded = false;
    }
}

void QLuaCommanderWidget::onGlobalFocusChanged(QWidget *old, QWidget *now)
{
    /* treat the statusbar focus as our own focus
     * why:
     * when options.resizeStatusbarWhenFocused is active, and we want to copy
     * text from the statusbar, we don't want to resize the statusbar as if
     * the widget has lost focus. */

    sim::addLog(sim_verbosity_debug, "focusChanged: old=%s, new=%s", old ? old->metaObject()->className() : "null", now ? now->metaObject()->className() : "null");

    QPlainTextEdit *statusBar = UIProxy::getInstance()->getStatusBar();

    if(old == editor && now == statusBar)
    {
        // do nothing
    }
    else if(now == editor)
    {
        onFocusIn();
    }
    else if(old == editor || old == statusBar)
    {
        onFocusOut();
    }
}

void QLuaCommanderWidget::onSetCompletion(const QStringList &comp)
{
    editor->setCompletion(comp);
}

void QLuaCommanderWidget::onSetCallTip(const QString &tip)
{
#ifdef CUSTOM_TOOLTIP_FOR_CALLTIPS
    if(tip.isEmpty())
    {
        calltipLabel->hide();
    }
    else
    {
        calltipLabel->setText(tip);
        calltipLabel->show();
    }
#else // CUSTOM_TOOLTIP_FOR_CALLTIPS
    editor->setToolTipAtCursor(tip);
#endif // CUSTOM_TOOLTIP_FOR_CALLTIPS
}

void QLuaCommanderWidget::onScriptListChanged(QMap<int,QString> childScripts, QMap<int,QString> customizationScripts, bool simRunning)
{
    // save current item:
    QVariant old = scriptCombo->itemData(scriptCombo->currentIndex());

    // clear cxombo box:
    while(scriptCombo->count()) scriptCombo->removeItem(0);

    // populate combo box:
    static boost::format childScriptFmt("Child script of '%s'");
    static boost::format customizationScriptFmt("Customization script of '%s'");
    int index = 0, selectedIndex = -1;
    {
        QVariantList data;
        data << sim_scripttype_sandboxscript << 0 << QString();
        scriptCombo->addItem("Sandbox script", data);
        if(data == old) selectedIndex = index;
        index++;
    }
    if(simRunning)
    {
        QVariantList data;
        data << sim_scripttype_mainscript << 0 << QString();
        scriptCombo->addItem("Main script", data);
        if(data == old) selectedIndex = index;
        index++;
    }
    if(simRunning)
    {
        QMapIterator<int,QString> i(childScripts);
        while(i.hasNext())
        {
            i.next();
            QVariantList data;
            data << sim_scripttype_childscript << i.key() << i.value();
            scriptCombo->addItem((childScriptFmt % i.value().toStdString()).str().c_str(), data);
            if(data == old) selectedIndex = index;
            index++;
        }
    }
    {
        QMapIterator<int,QString> i(customizationScripts);
        while(i.hasNext())
        {
            i.next();
            QVariantList data;
            data << sim_scripttype_customizationscript << i.key() << i.value();
            scriptCombo->addItem((customizationScriptFmt % i.value().toStdString()).str().c_str(), data);
            if(data == old) selectedIndex = index;
            index++;
        }
    }

    if(selectedIndex >= 0)
        scriptCombo->setCurrentIndex(selectedIndex);
    else
        scriptCombo->setCurrentIndex(0);
}

void QLuaCommanderWidget::setHistory(QStringList hist)
{
    editor->setHistory(hist);
}

