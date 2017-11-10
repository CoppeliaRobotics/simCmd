#include "QCommanderWidget.h"
#include "UIProxy.h"
#include "UIFunctions.h"
#include <boost/format.hpp>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QTimer>

QCommanderEditor::QCommanderEditor(QCommanderWidget *parent)
    : QLineEdit(parent),
      commander(parent)
{
    installEventFilter(this);
}

QCommanderEditor::~QCommanderEditor()
{
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
    setSelection(before.length(), (before + z).length());
}

void QCommanderEditor::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Escape)
    {
        emit escapePressed();
    }
    else if(event->key() == Qt::Key_Up)
    {
        emit upPressed();
    }
    else if(event->key() == Qt::Key_Down)
    {
        emit downPressed();
    }
    else
    {
        QLineEdit::keyPressEvent(event);
    }
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
                emit getNextCompletion(type, t, selectedText());
            return true;
        }
        if(keyEvent->key() == Qt::Key_Backtab)
        {
            commander->getSelectedScriptInfo(type, handle, name);
            QString t = tokenBehindCursor();
            if(t.length() > 0)
                emit getPrevCompletion(type, t, selectedText());
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}

void QCommanderEditor::moveCursorToEnd()
{
    setCursorPosition(text().size());
}

QCommanderWidget::QCommanderWidget(QWidget *parent)
    : QWidget(parent),
      historyIndex(0)
{
    editor = new QCommanderEditor(this);
    editor->setPlaceholderText("Input Lua code here (use TAB for auto-completion)");
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
    connect(closeButton, &QPushButton::clicked, this, &QCommanderWidget::onClose);
}

QCommanderWidget::~QCommanderWidget()
{
}

void QCommanderWidget::setHistoryIndex(int index)
{
    if(history.size() == 0) return;

    historyIndex = index;

    if(historyIndex < 0)
        historyIndex = 0;

    if(historyIndex > history.size() - 1)
        historyIndex = history.size() - 1;

    editor->setText(history[historyIndex]);
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

void QCommanderWidget::onReturnPressed()
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
}

void QCommanderWidget::onEscapePressed()
{
    historyIndex = history.size();
    editor->setText("");
}

void QCommanderWidget::onUpPressed()
{
    setHistoryIndex(--historyIndex);
}

void QCommanderWidget::onDownPressed()
{
    setHistoryIndex(++historyIndex);
}

void QCommanderWidget::onClose()
{
    closeFlag.store(true);
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
    if(simRunning) {
        QVariantList data;
        data << sim_scripttype_mainscript << 0 << QString();
        scriptCombo->addItem("Main script", data);
        if(data == old) selectedIndex = index;
        index++;
    }
    if(simRunning) {
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

