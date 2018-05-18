#include "QCommanderWidget.h"
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

QGlobalEventFilter::QGlobalEventFilter(QCommanderWidget *commander, QCommanderEditor *widget)
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

void QGlobalEventFilter::install(QCommanderWidget *commander, QCommanderEditor *widget)
{
    instance_ = new QGlobalEventFilter(commander, widget);
    QCoreApplication::instance()->installEventFilter(instance_);
}

void QGlobalEventFilter::uninstall()
{
    QCoreApplication::instance()->removeEventFilter(instance_);
}

QCommanderEditor::QCommanderEditor(QCommanderWidget *parent)
    : QLineEdit(parent),
      commander(parent)
{
    installEventFilter(this);
    QGlobalEventFilter::install(parent, this);
}

QCommanderEditor::~QCommanderEditor()
{
    QGlobalEventFilter::uninstall();
}

inline bool isID(QChar c)
{
    return c.isLetterOrNumber() || c == '_' || c == '.';
}

QString QCommanderEditor::tokenBehindCursor()
{
    QString t = text();
    int c = hasSelectedText() ? selectionStart() : cursorPosition();

    QString before = t.left(c);
    QString sel = selectedText();
    QString after = t.mid(c + sel.length());

    if(!after.isEmpty() && isID(after[0])) return QString();

    int j = before.length() - 1;
    while(j >= 0 && isID(before[j])) j--;
    return before.mid(j + 1);
}

void QCommanderEditor::setCompletion(QString s)
{
    QString b = tokenBehindCursor();
    if(!s.startsWith(b)) return;
    QString z = s.mid(b.length());
    QString t = text();
    int c = hasSelectedText() ? selectionStart() : cursorPosition();

    QString before = t.left(c);
    QString after = t.mid(c + selectedText().length());
    QString newText = before + z + after;
    setText(newText);
    setCursorPosition(before.length());
    setSelection(before.length(), z.length());
}

void QCommanderEditor::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Escape)
    {
        emit escapePressed();
        return;
    }
    else if(event->key() == Qt::Key_Up)
    {
        emit upPressed();
        return;
    }
    else if(event->key() == Qt::Key_Down)
    {
        emit downPressed();
        return;
    }
    else if(event->key() == Qt::Key_ParenLeft)
    {
        // if there is completion, accept it:
        if(hasSelectedText())
        {
            setCursorPosition(selectionStart() + selectedText().length());
        }

        // ask call tip:
        int type = sim_scripttype_sandboxscript;
        int handle = -1;
        QString name;
        commander->getSelectedScriptInfo(type, handle, name);

        // find symbol before '('
        QString symbol = text().left(cursorPosition());
        int j = symbol.length() - 1;
        while(j >= 0 && isID(symbol[j])) j--;
        symbol = symbol.mid(j + 1);

        emit askCallTip(type, symbol);
    }
    else if(event->key() == Qt::Key_Period)
    {
        // if there is completion, accept it:
        if(hasSelectedText())
        {
            setCursorPosition(selectionStart() + selectedText().length());
        }
    }
    else if(event->key() == Qt::Key_Comma)
    {
        // reshow last tooltip when comma is pressed
        setCallTip(toolTip());
    }
    else if(event->key() == Qt::Key_ParenRight)
    {
        setCallTip("");
    }
    else if(event->key() == Qt::Key_L && event->modifiers().testFlag(Q_REAL_CTRL))
    {
        emit clear();
        return;
    }
    QLineEdit::keyPressEvent(event);
}

bool QCommanderEditor::eventFilter(QObject *obj, QEvent *event)
{
    if(event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        int type = sim_scripttype_sandboxscript;
        int handle = -1;
        QString name;
        if(keyEvent->key() == Qt::Key_Tab)
        {
            commander->getSelectedScriptInfo(type, handle, name);
            QString t = tokenBehindCursor();
            if(t.length() > 0)
                emit getNextCompletion(type, name, t, selectedText());
            return true;
        }
        if(keyEvent->key() == Qt::Key_Backtab)
        {
            commander->getSelectedScriptInfo(type, handle, name);
            QString t = tokenBehindCursor();
            if(t.length() > 0)
                emit getPrevCompletion(type, name, t, selectedText());
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}

void QCommanderEditor::moveCursorToEnd()
{
    setCursorPosition(text().size());
}

void QCommanderEditor::setCallTip(QString tip)
{
    if(tip.isEmpty())
    {
        QToolTip::hideText();
    }
    else
    {
        setToolTip(tip);

        QFontMetrics fm(QToolTip::font());
        QRect r = fm.boundingRect(QRect(0, 0, 500, 50), 0, tip);

        QPoint cur = mapToGlobal(cursorRect().topLeft());
        QHelpEvent *event = new QHelpEvent(QEvent::ToolTip,
                QPoint(pos().x(), pos().y()),
                QPoint(cur.x(), cur.y() - height() - r.height() - 4));
        QApplication::postEvent(this, event);
    }
}

QCommanderWidget::QCommanderWidget(QWidget *parent)
    : QWidget(parent),
      historyIndex(0)
{
    statusbarSize = UIProxy::getInstance()->getStatusbarSize();
    const int k = 100;
    statusbarSizeFocused.push_back(statusbarSize[0] - k);
    statusbarSizeFocused.push_back(statusbarSize[1] + k);

    closeFlag.store(false);
    editor = new QCommanderEditor(this);
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
    connect(editor, &QCommanderEditor::returnPressed, this, &QCommanderWidget::onReturnPressed);
    connect(editor, &QCommanderEditor::escapePressed, this, &QCommanderWidget::onEscapePressed);
    connect(editor, &QCommanderEditor::upPressed, this, &QCommanderWidget::onUpPressed);
    connect(editor, &QCommanderEditor::downPressed, this, &QCommanderWidget::onDownPressed);
    connect(editor, &QCommanderEditor::clear, this, &QCommanderWidget::onClear);
    connect(editor, &QCommanderEditor::textEdited, this, &QCommanderWidget::onTextEdited);
    connect(closeButton, &QPushButton::clicked, this, &QCommanderWidget::onClose);
    connect(QApplication::instance(), SIGNAL(focusChanged(QWidget*,QWidget*)), this, SLOT(onGlobalFocusChanged(QWidget*,QWidget*)));
}

QCommanderWidget::~QCommanderWidget()
{
}

void QCommanderWidget::setOptions(const PersistentOptions &options_)
{
    options = options_;
}

void QCommanderWidget::setHistoryIndex(int index)
{
    if(history.isEmpty())
    {
        DBG << "history is empty" << std::endl;
        return;
    }

    historyIndex = index;

    if(historyIndex < 0)
        historyIndex = 0;

    if(historyIndex > history.size() - 1)
        historyIndex = history.size() - 1;

    DBG << "setting historyIndex=" << historyIndex
        << " \"" << history[historyIndex].toStdString() << "\"" << std::endl;

    editor->setText(history[historyIndex]);
    editor->setCallTip("");
    QTimer::singleShot(0, editor, &QCommanderEditor::moveCursorToEnd);
}

bool QCommanderWidget::getSelectedScriptInfo(int &type, int &handle, QString &name)
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

void QCommanderWidget::execute()
{
    QString code = editor->text();
    int type = sim_scripttype_sandboxscript;
    int handle = -1;
    QString name = "";
    getSelectedScriptInfo(type, handle, name);
    emit execCode(code, type, name);
    history << code;
    historyIndex = history.size();
    editor->setText("");
    editor->setCallTip("");
    historyPrefixFilter = "";
}

void QCommanderWidget::acceptCompletion()
{
    if(editor->hasSelectedText())
    {
        editor->setCursorPosition(editor->selectionStart() + editor->selectedText().length());
        editor->setCallTip("");
    }
}

void QCommanderWidget::onReturnPressed()
{
    if(editor->hasSelectedText())
        acceptCompletion();
    else
        execute();
}

void QCommanderWidget::onEscapePressed()
{
    if(editor->text().isEmpty())
    {
        // if text is already empty, clear widget focus
        editor->clearFocus();
        return;
    }

    historyIndex = history.size();
    editor->setText("");
    editor->setCallTip("");
    historyPrefixFilter = "";
    DBG << "historyPrefixFilter=" << historyPrefixFilter.toStdString() << std::endl;
}

void QCommanderWidget::onUpPressed()
{
    if(history.isEmpty()) return;

    if(!historyPrefixFilter.isEmpty())
    {
        DBG << "historyPrefixFilter=" << historyPrefixFilter.toStdString()
            << ", old historyIndex=" << historyIndex << std::endl;

        // find previous history entry matching historyPrefixFilter
        int i = historyIndex - 1;
        while(i >= 0 && i < history.size() && !history[i].startsWith(historyPrefixFilter))
            --i;
        if(i >= 0 && i < history.size() && history[i].startsWith(historyPrefixFilter))
            setHistoryIndex(i);

        DBG << "new historyIndex=" << historyIndex << std::endl;
    }
    else
    {
        setHistoryIndex(--historyIndex);
    }
}

void QCommanderWidget::onDownPressed()
{
    if(history.isEmpty()) return;

    if(!historyPrefixFilter.isEmpty())
    {
        DBG << "historyPrefixFilter=" << historyPrefixFilter.toStdString()
            << ", old historyIndex=" << historyIndex << std::endl;

        // find next history entry matching historyPrefixFilter
        int i = historyIndex + 1;
        while(i >= 0 && i < history.size() && !history[i].startsWith(historyPrefixFilter))
            ++i;
        if(i >= 0 && i < history.size() && history[i].startsWith(historyPrefixFilter))
            setHistoryIndex(i);

        DBG << "new historyIndex=" << historyIndex << std::endl;
    }
    else
    {
        setHistoryIndex(++historyIndex);
    }
}

void QCommanderWidget::onClose()
{
    closeFlag.store(true);
}

void QCommanderWidget::onClear()
{
    simAddStatusbarMessage(NULL);
}

void QCommanderWidget::onTextEdited()
{
    historyPrefixFilter = editor->text();
    DBG << "historyPrefixFilter=" << historyPrefixFilter.toStdString() << std::endl;
}

void QCommanderWidget::onFocusIn()
{
    if(options.resizeStatusbarWhenFocused)
        expandStatusbar();
}

void QCommanderWidget::onFocusOut()
{
    if(options.resizeStatusbarWhenFocused)
        contractStatusbar();
}

void QCommanderWidget::expandStatusbar()
{
    if(!statusbarExpanded)
    {
        statusbarSize = UIProxy::getInstance()->getStatusbarSize();
        UIProxy::getInstance()->setStatusbarSize(statusbarSizeFocused);
        statusbarExpanded = true;
    }
}

void QCommanderWidget::contractStatusbar()
{
    if(statusbarExpanded)
    {
        statusbarSizeFocused = UIProxy::getInstance()->getStatusbarSize();
        UIProxy::getInstance()->setStatusbarSize(statusbarSize);
        statusbarExpanded = false;
    }
}

void QCommanderWidget::onGlobalFocusChanged(QWidget *old, QWidget *now)
{
    /* treat the statusbar focus as our own focus
     * why:
     * when options.resizeStatusbarWhenFocused is active, and we want to copy
     * text from the statusbar, we don't want to resize the statusbar as if
     * the widget has lost focus. */

    DBG << "focusChanged: old=" << (old ? old->metaObject()->className() : "null")
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

void QCommanderWidget::onScriptListChanged(QMap<int,QString> childScripts, QMap<int,QString> customizationScripts, bool simRunning)
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

void QCommanderWidget::setHistory(QStringList hist)
{
    history = hist;
    historyIndex = history.size();
}

