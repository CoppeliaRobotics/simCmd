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
    const int k = (statusbarSize[0] + statusbarSize[1]) / 2;
    statusbarSizeFocused.push_back(k);
    statusbarSizeFocused.push_back(k);

    QGridLayout *grid = new QGridLayout(this);
    grid->setSpacing(0);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    grid->setMargin(0);
#endif
    grid->setContentsMargins(0, 0, 0, 0);
    setLayout(grid);

#ifdef CUSTOM_TOOLTIP_WINDOW
    calltipLabel = new QLabel(this);
    calltipLabel->setVisible(false);
#endif // CUSTOM_TOOLTIP_WINDOW
    editor = new QCommanderEdit(this);
    editor->setPlaceholderText("Input code here, or type \"help()\" (use TAB for auto-completion)");
    editor->setFont(QFont("Courier", 12));
    scriptCombo = new QComboBox(this);
    scriptCombo->setMinimumContentsLength(20);
    scriptCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);

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
    connect(SIM::getInstance(), &SIM::toggleStatusbarHeight, this, &QCommanderWidget::toggleStatusbarHeight);
}

QCommanderWidget::~QCommanderWidget()
{
}

void QCommanderWidget::getSelectedScriptInfo(int &type, int &handle)
{
    type = -1;
    handle = -1;

    if(scriptCombo->currentIndex() >= 0)
    {
        QVariantList data = scriptCombo->itemData(scriptCombo->currentIndex()).toList();
        type = data[0].toInt();
        handle = data[1].toInt();
    }
}

void QCommanderWidget::onAskCompletion(const QString &cmd, int cursorPos)
{
    int scriptType;
    int scriptHandle;
    getSelectedScriptInfo(scriptType, scriptHandle);
    if(scriptHandle != -1)
        emit askCompletion(scriptHandle, cmd, cursorPos, nullptr);
}

void QCommanderWidget::onAskCallTip(QString input, int pos)
{
    int scriptType;
    int scriptHandle;
    getSelectedScriptInfo(scriptType, scriptHandle);
    if(scriptHandle != -1)
        emit askCallTip(scriptHandle, input, pos);
}

void QCommanderWidget::onExecute(const QString &cmd)
{
    int scriptType;
    int scriptHandle;
    QString lang;
    getSelectedScriptInfo(scriptType, scriptHandle);

    if(scriptHandle != -1)
        emit execCode(scriptHandle, cmd);
    else
        emit addLog(sim_verbosity_errors, "No script is selected");

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

bool QCommanderWidget::statusbarExpanded()
{
    auto s = UI::getInstance()->getStatusbarSize();
    int thr = 3 * (s[0] + s[1]) / 4;
    return s[0] < thr;
}

void QCommanderWidget::expandStatusbar()
{
    UI::getInstance()->setStatusbarSize(statusbarSizeFocused);
}

void QCommanderWidget::contractStatusbar()
{
    UI::getInstance()->setStatusbarSize(statusbarSize);
}

void QCommanderWidget::toggleStatusbarHeight()
{
    if(statusbarExpanded())
        contractStatusbar();
    else
        expandStatusbar();
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

void QCommanderWidget::onScriptListChanged(QMap<int,QString> sandboxScripts, int mainScript, QMap<int,QString> simulationScripts, QMap<int,QString> customizationScripts, QMap<int,QString> addons, bool simRunning, bool isRunningJustChanged)
{
    // save current selection:
    int oldScriptType;
    int oldScriptHandle;
    getSelectedScriptInfo(oldScriptType, oldScriptHandle);

    // clear cxombo box:
    while(scriptCombo->count()) scriptCombo->removeItem(0);

    // populate combo box:
    bool sep = false;
    if(true)
    {
        for(const auto &e : sandboxScripts.toStdMap())
        {
            if(oldScriptHandle == -1)
            {
                // start with the first sandbox script selected
                oldScriptHandle = e.first;
            }

            QVariantList data;
            data << sim_scripttype_sandbox << e.first << e.second;
            scriptCombo->addItem(e.second, data);
        }
    }
    if(simRunning)
    {
        QVariantList data;
        data << sim_scripttype_main << mainScript << QString();
        if(!sep) { sep = true; scriptCombo->insertSeparator(scriptCombo->count()); }
        scriptCombo->addItem("Main script", data);
    }
    if(simRunning)
    {
        for(const auto &e : simulationScripts.toStdMap())
        {
            QVariantList data;
            data << sim_scripttype_simulation << e.first << e.second;
            if(!sep) { sep = true; scriptCombo->insertSeparator(scriptCombo->count()); }
            scriptCombo->addItem(e.second, data);
        }
    }
    {
        for(const auto &e : customizationScripts.toStdMap())
        {
            QVariantList data;
            data << sim_scripttype_customization << e.first << e.second;
            if(!sep) { sep = true; scriptCombo->insertSeparator(scriptCombo->count()); }
            scriptCombo->addItem(e.second, data);
        }
    }
    {
        QList<QPair<int, QString>> sortedAddons;
        for(auto it = addons.begin(); it != addons.end(); ++it)
            sortedAddons.append(qMakePair(it.key(), it.value()));
        std::sort(sortedAddons.begin(), sortedAddons.end(), [](const QPair<int, QString>& a, const QPair<int, QString>& b) {
            return a.second < b.second;
        });

        sep = false;
        for(const auto &e : sortedAddons)
        {
            QVariantList data;
            data << sim_scripttype_addon << e.first << e.second;
            if(!sep) { sep = true; scriptCombo->insertSeparator(scriptCombo->count()); }
            scriptCombo->addItem(e.second, data);
        }
    }

    // restore selection:
    setSelectedScript(oldScriptHandle, true, isRunningJustChanged);
}

void QCommanderWidget::setHistory(QStringList hist)
{
    editor->setHistory(hist);
}

void QCommanderWidget::setAutoAcceptCommonCompletionPrefix(bool b)
{
    editor->setAutoAcceptLongestCommonCompletionPrefix(b);
}

void QCommanderWidget::setShowMatchingHistory(bool b)
{
    editor->setShowMatchingHistory(b);
}

void QCommanderWidget::setSelectedScript(int newScriptHandle, bool silent, bool fallbackToSandbox)
{
    int index = -1;
    bool found = false;
    for(int i = 0; i < scriptCombo->count(); i++)
    {
        QVariantList data = scriptCombo->itemData(i).toList();
        if(data.isEmpty()) continue;
        int scriptHandle = data[1].toInt();
        if(scriptHandle == newScriptHandle)
        {
            index = i;
            found = true;
            break;
        }
    }

    if(index != scriptCombo->currentIndex())
    {
        scriptCombo->setCurrentIndex(index);
    }

    if(!found && fallbackToSandbox)
        setSelectedScript(-1, true, false);
}
