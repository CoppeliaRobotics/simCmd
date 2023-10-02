#include "qluacommanderwidget.h"
#include "UI.h"
#include "SIM.h"
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

        emit askCallTip(text(), cursorPosition());
    }
    else if(event->key() == Qt::Key_Period)
    {
        acceptCompletion();
    }
    else if(event->key() == Qt::Key_Comma)
    {
        // reshow last tooltip when comma is pressed
        w->onSetCallTip(toolTip());
    }
    else if(event->key() == Qt::Key_ParenRight)
    {
        w->onSetCallTip("");
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
    statusbarSize = UI::getInstance()->getStatusbarSize();
    const int k = 100;
    statusbarSizeFocused.push_back(statusbarSize[0] - k);
    statusbarSizeFocused.push_back(statusbarSize[1] + k);

    closeFlag.store(false);

    QGridLayout *grid = new QGridLayout(this);
    grid->setSpacing(0);
    grid->setMargin(0);
    setLayout(grid);

#ifdef CUSTOM_TOOLTIP_WINDOW
    calltipLabel = new QLabel(this);
    calltipLabel->setVisible(false);
#endif // CUSTOM_TOOLTIP_WINDOW
    editor = new QLuaCommanderEdit(this);
    editor->setPlaceholderText("Input code here, or type \"help()\" (use TAB for auto-completion)");
    editor->setFont(QFont("Courier", 12));
    scriptCombo = new QComboBox(this);
    scriptCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    closeButton = new QPushButton(this);
    closeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    closeButton->setFlat(true);
    closeButton->setStyleSheet("margin-left: 5px; margin-right: 5px; font-size: 14pt;");

#ifdef CUSTOM_TOOLTIP_WINDOW
    grid->addWidget(calltipLabel, 0, 0);
#endif // CUSTOM_TOOLTIP_WINDOW
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

void QLuaCommanderWidget::getSelectedScriptInfo(int &handle, QString &langSuffix)
{
    handle = -1;
    langSuffix = "";

    if(scriptCombo->currentIndex() >= 0)
    {
        QVariantList data = scriptCombo->itemData(scriptCombo->currentIndex()).toList();
        handle = data[1].toInt();
        langSuffix = data[3].toString();
    }
}

void QLuaCommanderWidget::onAskCompletion(const QString &cmd, int cursorPos)
{
    int scriptHandle;
    QString langSuffix;
    getSelectedScriptInfo(scriptHandle, langSuffix);
    emit askCompletion(scriptHandle, langSuffix, cmd, cursorPos, nullptr);
}

void QLuaCommanderWidget::onAskCallTip(QString input, int pos)
{
    int scriptHandle;
    QString langSuffix;
    getSelectedScriptInfo(scriptHandle, langSuffix);
    emit askCallTip(scriptHandle, langSuffix, input, pos);
}

void QLuaCommanderWidget::onExecute(const QString &cmd)
{
    int scriptHandle;
    QString langSuffix;
    getSelectedScriptInfo(scriptHandle, langSuffix);
    emit execCode(scriptHandle, langSuffix, cmd);
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
    sim::addLog({}, 0, {});
}

void QLuaCommanderWidget::onFocusIn()
{
    if(resizeStatusbarWhenFocused)
        expandStatusbar();
}

void QLuaCommanderWidget::onFocusOut()
{
    if(resizeStatusbarWhenFocused)
        contractStatusbar();
}

void QLuaCommanderWidget::expandStatusbar()
{
    if(!statusbarExpanded)
    {
        statusbarSize = UI::getInstance()->getStatusbarSize();
        UI::getInstance()->setStatusbarSize(statusbarSizeFocused);
        statusbarExpanded = true;
    }
}

void QLuaCommanderWidget::contractStatusbar()
{
    if(statusbarExpanded)
    {
        statusbarSizeFocused = UI::getInstance()->getStatusbarSize();
        UI::getInstance()->setStatusbarSize(statusbarSize);
        statusbarExpanded = false;
    }
}

void QLuaCommanderWidget::onGlobalFocusChanged(QWidget *old, QWidget *now)
{
    /* treat the statusbar focus as our own focus
     * why:
     * when resizeStatusbarWhenFocused is active, and we want to copy
     * text from the statusbar, we don't want to resize the statusbar as if
     * the widget has lost focus. */

    sim::addLog(sim_verbosity_debug, "focusChanged: old=%s, new=%s", old ? old->metaObject()->className() : "null", now ? now->metaObject()->className() : "null");

    QPlainTextEdit *statusBar = UI::getInstance()->getStatusBar();

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
#ifdef CUSTOM_TOOLTIP_WINDOW
    if(tip.isEmpty())
    {
        calltipLabel->hide();
    }
    else
    {
        calltipLabel->setText(tip);
        calltipLabel->show();
    }
#else // CUSTOM_TOOLTIP_WINDOW
    editor->setToolTipAtCursor(tip);
#endif // CUSTOM_TOOLTIP_WINDOW
}

void QLuaCommanderWidget::onScriptListChanged(int sandboxScript, int mainScript, QMap<int,QString> childScripts, QMap<int,QString> customizationScripts, bool simRunning)
{
    // save current item:
    QVariant old = scriptCombo->itemData(scriptCombo->currentIndex());

    // clear cxombo box:
    while(scriptCombo->count()) scriptCombo->removeItem(0);

    // populate combo box:
    int index = 0, selectedIndex = -1;
    QMap<QString, QString> sandboxLangs{{"Lua", "@lua"}, {"Python", "@python"}};
    for(int i = 0; i < 2; i++)
    {
        for(const auto &e : sandboxLangs.toStdMap())
        {
            QString lang = e.first;
            if((i == 0) ^ (lang == preferredSandboxLang)) continue;
            QString suffix = e.second;
            QVariantList data;
            data << sim_scripttype_sandboxscript << sandboxScript << QString() << suffix;
            scriptCombo->addItem(QString("Sandbox script (%1)").arg(lang), data);
            if(data == old) selectedIndex = index;
            index++;
        }
    }
    if(simRunning)
    {
        QVariantList data;
        data << sim_scripttype_mainscript << mainScript << QString() << QString();
        scriptCombo->addItem("Main script", data);
        if(data == old) selectedIndex = index;
        index++;
    }
    if(simRunning)
    {
        for(const auto &e : childScripts.toStdMap())
        {
            QVariantList data;
            data << sim_scripttype_childscript << e.first << e.second << QString();
            scriptCombo->addItem(e.second, data);
            if(data == old) selectedIndex = index;
            index++;
        }
    }
    {
        for(const auto &e : customizationScripts.toStdMap())
        {
            QVariantList data;
            data << sim_scripttype_customizationscript << e.first << e.second << QString();
            scriptCombo->addItem(e.second, data);
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

void QLuaCommanderWidget::setResizeStatusbarWhenFocused(bool b)
{
    resizeStatusbarWhenFocused = b;
}

void QLuaCommanderWidget::setPreferredSandboxLang(const QString &lang)
{
    preferredSandboxLang = lang;
}

void QLuaCommanderWidget::setAutoAcceptCommonCompletionPrefix(bool b)
{
    editor->setAutoAcceptLongestCommonCompletionPrefix(b);
}

void QLuaCommanderWidget::setShowMatchingHistory(bool b)
{
    editor->setShowMatchingHistory(b);
}
