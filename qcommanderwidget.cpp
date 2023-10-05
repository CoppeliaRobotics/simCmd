#include "qcommanderwidget.h"
#include "UI.h"
#include "SIM.h"
#include <boost/format.hpp>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QTimer>
#include <QToolTip>
#include <QApplication>
#include <QLabel>
#include <QStandardItem>
#include <QStandardItemModel>

#ifdef Q_OS_MACOS
#define Q_REAL_CTRL Qt::MetaModifier
#else
#define Q_REAL_CTRL Qt::ControlModifier
#endif

QGlobalEventFilter * QGlobalEventFilter::instance_ = NULL;

QGlobalEventFilter::QGlobalEventFilter(QCommanderWidget *commander, QCommanderEdit *widget)
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

void QGlobalEventFilter::install(QCommanderWidget *commander, QCommanderEdit *widget)
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

QCommanderEdit::QCommanderEdit(QCommanderWidget *parent)
    : QCommandEdit(parent),
      commander(parent)
{
    QGlobalEventFilter::install(parent, this);
}

QCommanderEdit::~QCommanderEdit()
{
    QGlobalEventFilter::uninstall();
}

void QCommanderEdit::keyPressEvent(QKeyEvent *event)
{
    auto w = static_cast<QCommanderWidget*>(parent());

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

QCommanderWidget::QCommanderWidget(QWidget *parent)
    : QWidget(parent)
{
    statusbarSize = UI::getInstance()->getStatusbarSize();
    const int k = 100;
    statusbarSizeFocused.push_back(statusbarSize[0] - k);
    statusbarSizeFocused.push_back(statusbarSize[1] + k);

    QGridLayout *grid = new QGridLayout(this);
    grid->setSpacing(0);
    grid->setMargin(0);
    setLayout(grid);

#ifdef CUSTOM_TOOLTIP_WINDOW
    calltipLabel = new QLabel(this);
    calltipLabel->setVisible(false);
#endif // CUSTOM_TOOLTIP_WINDOW
    editor = new QCommanderEdit(this);
    editor->setPlaceholderText("Input code here, or type \"help()\" (use TAB for auto-completion)");
    editor->setFont(QFont("Courier", 12));
    scriptCombo = new QComboBox(this);
    scriptCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);

#ifdef CUSTOM_TOOLTIP_WINDOW
    grid->addWidget(calltipLabel, 0, 0);
#endif // CUSTOM_TOOLTIP_WINDOW
    grid->addWidget(editor, 1, 0);
    grid->addWidget(scriptCombo, 1, 1);

    connect(editor, &QCommanderEdit::askCompletion, this, &QCommanderWidget::onAskCompletion);
    connect(editor, &QCommanderEdit::askCallTip, this, &QCommanderWidget::onAskCallTip);
    connect(editor, &QCommanderEdit::execute, this, &QCommanderWidget::onExecute);
    connect(editor, &QCommanderEdit::escape, this, &QCommanderWidget::onEscape);
    connect(editor, &QCommanderEdit::editorCleared, this, &QCommanderWidget::onEditorCleared);
    connect(editor, &QCommanderEdit::textChanged, this, &QCommanderWidget::onEditorChanged);
    connect(editor, &QCommanderEdit::cursorPositionChanged, this, &QCommanderWidget::onEditorCursorChanged);
    connect(editor, &QCommanderEdit::clearConsole, this, &QCommanderWidget::onClearConsole);
    connect(QApplication::instance(), SIGNAL(focusChanged(QWidget*,QWidget*)), this, SLOT(onGlobalFocusChanged(QWidget*,QWidget*)));
}

QCommanderWidget::~QCommanderWidget()
{
}

void QCommanderWidget::getSelectedScriptInfo(int &type, int &handle, QString &langSuffix)
{
    type = -1;
    handle = -1;
    langSuffix = "";

    if(scriptCombo->currentIndex() >= 0)
    {
        QVariantList data = scriptCombo->itemData(scriptCombo->currentIndex()).toList();
        type = data[0].toInt();
        handle = data[1].toInt();
        langSuffix = data[3].toString();
    }
}

void QCommanderWidget::onAskCompletion(const QString &cmd, int cursorPos)
{
    int scriptType;
    int scriptHandle;
    QString langSuffix;
    getSelectedScriptInfo(scriptType, scriptHandle, langSuffix);
    emit askCompletion(scriptHandle, langSuffix, cmd, cursorPos, nullptr);
}

void QCommanderWidget::onAskCallTip(QString input, int pos)
{
    int scriptType;
    int scriptHandle;
    QString langSuffix;
    getSelectedScriptInfo(scriptType, scriptHandle, langSuffix);
    emit askCallTip(scriptHandle, langSuffix, input, pos);
}

void QCommanderWidget::onExecute(const QString &cmd)
{
    int scriptType;
    int scriptHandle;
    QString langSuffix;
    getSelectedScriptInfo(scriptType, scriptHandle, langSuffix);

    if(cmd.length() > 1 && QString("@lua").startsWith(cmd))
    {
        if(scriptType == sim_scripttype_sandboxscript)
            setSelectedScript(-1, "Lua");
    }
    else if(cmd.length() > 1 && QString("@python").startsWith(cmd))
    {
        if(scriptType == sim_scripttype_sandboxscript)
            setSelectedScript(-1, "Python");
    }
    else
    {
        emit execCode(scriptHandle, langSuffix, cmd);
    }

    editor->clear();
    onSetCallTip("");
}

void QCommanderWidget::onEscape()
{
    editor->clearFocus();
}

void QCommanderWidget::onEditorCleared()
{
    onSetCallTip("");
}

void QCommanderWidget::onEditorChanged(QString text)
{
    onAskCallTip(text, editor->cursorPosition());
}

void QCommanderWidget::onEditorCursorChanged(int oldPos, int newPos)
{
    onAskCallTip(editor->text(), newPos);
}

void QCommanderWidget::onClearConsole()
{
    sim::addLog({}, 0, {});
}

void QCommanderWidget::onFocusIn()
{
    if(resizeStatusbarWhenFocused)
        expandStatusbar();
}

void QCommanderWidget::onFocusOut()
{
    if(resizeStatusbarWhenFocused)
        contractStatusbar();
}

void QCommanderWidget::expandStatusbar()
{
    if(!statusbarExpanded)
    {
        statusbarSize = UI::getInstance()->getStatusbarSize();
        UI::getInstance()->setStatusbarSize(statusbarSizeFocused);
        statusbarExpanded = true;
    }
}

void QCommanderWidget::contractStatusbar()
{
    if(statusbarExpanded)
    {
        statusbarSizeFocused = UI::getInstance()->getStatusbarSize();
        UI::getInstance()->setStatusbarSize(statusbarSize);
        statusbarExpanded = false;
    }
}

void QCommanderWidget::onGlobalFocusChanged(QWidget *old, QWidget *now)
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

void QCommanderWidget::onSetCompletion(const QStringList &comp)
{
    editor->setCompletion(comp);
}

void QCommanderWidget::onSetCallTip(const QString &tip)
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

void QCommanderWidget::onScriptListChanged(int sandboxScript_, int mainScript, QMap<int,QString> childScripts, QMap<int,QString> customizationScripts, bool simRunning, bool havePython_)
{
    havePython = havePython_;
    sandboxScript = sandboxScript_;

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
            if((i == 0) ^ (lang.toLower() == preferredSandboxLang.toLower())) continue;
            QString suffix = e.second;
            QVariantList data;
            data << sim_scripttype_sandboxscript << sandboxScript << QString() << suffix;
            scriptCombo->addItem(QString("Sandbox script (%1)").arg(lang), data);
            if(data == old) selectedIndex = index;
            if(lang == "Python" && !havePython)
            {
                QStandardItemModel *model = qobject_cast<QStandardItemModel*>(scriptCombo->model());
                model->item(index)->setEnabled(false);
            }
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

void QCommanderWidget::setHistory(QStringList hist)
{
    editor->setHistory(hist);
}

void QCommanderWidget::setResizeStatusbarWhenFocused(bool b)
{
    resizeStatusbarWhenFocused = b;
}

void QCommanderWidget::setPreferredSandboxLang(const QString &lang)
{
    preferredSandboxLang = lang.left(1).toUpper() + lang.mid(1).toLower();
}

void QCommanderWidget::setAutoAcceptCommonCompletionPrefix(bool b)
{
    editor->setAutoAcceptLongestCommonCompletionPrefix(b);
}

void QCommanderWidget::setShowMatchingHistory(bool b)
{
    editor->setShowMatchingHistory(b);
}

void QCommanderWidget::setSelectedScript(int scriptHandle, QString lang)
{
    lang = lang.left(1).toUpper() + lang.mid(1).toLower();

    if(lang.toLower() == "python" && !havePython)
    {
        emit addLog(sim_verbosity_errors, "Python is not available");
        return;
    }

    int index = -1;
    for(int i = 0; i < scriptCombo->count(); i++)
    {
        QVariantList data = scriptCombo->itemData(i).toList();
        int h = data[1].toInt();
        if(scriptHandle != -1 && scriptHandle != h) continue;
        QString suffix = data[3].toString();
        if(lang != "" && ("@" + lang.toLower()) != suffix) continue;
        index = i;
        break;
    }
    if(index != -1 && index != scriptCombo->currentIndex())
    {
        scriptCombo->setCurrentIndex(index);
        if(scriptHandle == sandboxScript)
            emit addLog(sim_verbosity_warnings, "Sandbox language: " + lang);
    }
}
