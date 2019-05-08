#include "qluacommanderwidget.h"
#include "UIProxy.h"
#include "UIFunctions.h"
#include "debug.h"
#include <boost/format.hpp>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QTimer>
#include <QToolTip>
#include <QApplication>

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

inline bool isID(QChar c)
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
    if(j > 0 && (before[j - 1] == '\'' || before[j - 1] == '\"'))
    {
        *ctx = 's'; // string
        if(j > 1 && before[j - 2] == 'H')
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
    if(event->key() == Qt::Key_ParenLeft)
    {
        acceptCompletion();

        // find symbol before '('
        QString symbol = text().left(cursorPosition());
        int j = symbol.length() - 1;
        while(j >= 0 && isID(symbol[j])) j--;
        symbol = symbol.mid(j + 1);

        emit askCallTip(symbol);
    }
    else if(event->key() == Qt::Key_Period)
    {
        acceptCompletion();
    }
    else if(event->key() == Qt::Key_Comma)
    {
        // reshow last tooltip when comma is pressed
        setToolTipAtCursor(toolTip());
    }
    else if(event->key() == Qt::Key_ParenRight)
    {
        setToolTipAtCursor("");
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
    editor = new QLuaCommanderEdit(this);
    editor->setPlaceholderText("Input Lua code here, or type \"help()\" (use TAB for auto-completion)");
    editor->setFont(QFont("Courier", 12));
    scriptCombo = new QComboBox(this);
    scriptCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    closeButton = new QPushButton(this);
    closeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    closeButton->setFlat(true);
    closeButton->setStyleSheet("margin-left: 5px; margin-right: 5px; font-size: 14pt;");
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setSpacing(0);
    layout->setMargin(0);
    setLayout(layout);
    layout->addWidget(editor);
    layout->addWidget(scriptCombo);
    layout->addWidget(closeButton);
    connect(editor, &QLuaCommanderEdit::askCompletion, this, &QLuaCommanderWidget::onAskCompletion);
    connect(editor, &QLuaCommanderEdit::askCallTip, this, &QLuaCommanderWidget::onAskCallTip);
    connect(editor, &QLuaCommanderEdit::execute, this, &QLuaCommanderWidget::onExecute);
    connect(editor, &QLuaCommanderEdit::escape, this, &QLuaCommanderWidget::onEscape);
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
        DEBUG_OUT << "cmd=\"" << cmd.toStdString() << "\", "
            "pos=" << cursorPos << ", "
            "tbc=" << token.toStdString() << ", "
            "ctx=" << context.toLatin1() << std::endl;

        emit askCompletion(type, name, token, context);
    }
}

void QLuaCommanderWidget::onAskCallTip(QString symbol)
{
    int type = sim_scripttype_sandboxscript;
    int handle = -1;
    QString name;
    getSelectedScriptInfo(type, handle, name);
    emit askCallTip(type, symbol);
}

void QLuaCommanderWidget::onExecute(const QString &cmd)
{
    int type = sim_scripttype_sandboxscript;
    int handle = -1;
    QString name;
    getSelectedScriptInfo(type, handle, name);
    emit execCode(cmd, type, name);
    editor->clear();
}

void QLuaCommanderWidget::onEscape()
{
    editor->clearFocus();
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

    DEBUG_OUT << "focusChanged: old=" << (old ? old->metaObject()->className() : "null")
        << ", new=" << (now ? now->metaObject()->className() : "null") << std::endl;

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
    editor->setToolTipAtCursor(tip);
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

