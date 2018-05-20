#include "qcommandedit.h"

#include <QApplication>
#include <QTimer>
#include <QTextLayout>
#include <QPainter>
#include <QToolTip>

QCommandEdit::QCommandEdit(QWidget *parent)
    : QLineEdit(parent),
      showMatchingHistory_(false)
{
    historyState_.reset();
    completionState_.reset();

    connect(this, &QCommandEdit::returnPressed, this, &QCommandEdit::onReturnPressed);
    connect(this, &QCommandEdit::escapePressed, this, &QCommandEdit::onEscapePressed);
    connect(this, &QCommandEdit::upPressed, this, &QCommandEdit::onUpPressed);
    connect(this, &QCommandEdit::downPressed, this, &QCommandEdit::onDownPressed);
    connect(this, &QCommandEdit::tabPressed, this, &QCommandEdit::onTabPressed);
    connect(this, &QCommandEdit::shiftTabPressed, this, &QCommandEdit::onShiftTabPressed);
    connect(this, &QCommandEdit::textEdited, this, &QCommandEdit::onTextEdited);
    connect(this, &QCommandEdit::selectionChanged, this, &QCommandEdit::onSelectionChanged);
    connect(this, &QCommandEdit::cursorPositionChanged, this, &QCommandEdit::onCursorPositionChanged);

    installEventFilter(this);
}

void QCommandEdit::setShowMatchingHistory(bool show)
{
    showMatchingHistory_ = show;
    if(show)
        searchMatchingHistoryAndShowGhost();
}

void QCommandEdit::paintEvent(QPaintEvent *event)
{
    QLineEdit::paintEvent(event);

    /* show ghost suffix. only shown if:
     * - widget has focus
     * - cursor is at end
     * - there is some text
     */

    if(!hasFocus()) return;
    if(ghostSuffix_.isEmpty()) return;
    QString txt = text();
    if(txt.isEmpty()) return;
    if(cursorPosition() < txt.length()) return;

    ensurePolished();
    QRect cr = cursorRect();
    QPoint pos = cr.topRight() - QPoint(cr.width() / 2, 0);
    QPainter p(this);
    p.setPen(QPen(Qt::gray, 1));
    QTextLayout l(ghostSuffix_, font());
    l.beginLayout();
    QTextLine line = l.createLine();
    line.setLineWidth(width() - pos.x());
    line.setPosition(pos);
    l.endLayout();
    l.draw(&p, QPoint(0, 0));
}

void QCommandEdit::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Escape)
    {
        emit escapePressed();
        return;
    }
    if(event->key() == Qt::Key_Up)
    {
        emit upPressed();
        return;
    }
    if(event->key() == Qt::Key_Down)
    {
        emit downPressed();
        return;
    }
    QLineEdit::keyPressEvent(event);
}

bool QCommandEdit::eventFilter(QObject *obj, QEvent *event)
{
    if(event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if(keyEvent->key() == Qt::Key_Tab)
        {
            emit tabPressed();
            return true;
        }
        if(keyEvent->key() == Qt::Key_Backtab)
        {
            emit shiftTabPressed();
            return true;
        }
    }
    return QLineEdit::eventFilter(obj, event);
}

/*!
 * \brief Clear the text and reset history/completion states
 */
void QCommandEdit::clear()
{
    setText("");
    ghostSuffix_ = "";
    historyState_.reset();
    completionState_.reset();
    setToolTipAtCursor("");
}

/*!
 * \brief Replacew the history content
 * \param history The new history content
 */
void QCommandEdit::setHistory(const QStringList &history)
{
    if(historyState_.index_ != -1)
        clear();
    historyState_.history_ = history;
    historyState_.reset();
}

/*!
 * \brief Navigate thru command history
 * \param delta 1 to go forward or -1 to go backward
 */
void QCommandEdit::navigateHistory(int delta)
{
    if(delta == 0) return;

    // clip delta to +1/-1:
    delta = (delta < -1 ? -1 : delta > 1 ? 1 : delta);

    // compute actual index (-1 => last):
    int newIndex = historyState_.index_;
    if(newIndex == -1) newIndex = historyState_.history_.length();

    if(historyState_.prefixFilter_.isEmpty())
    {
        // simply navigate up/down
        setHistoryIndex(newIndex + delta);
        return;
    }

    // search matching history
    while(1)
    {
        newIndex += delta;
        if(newIndex < 0 || newIndex >= historyState_.history_.length())
            break;
        if(historyState_.history_[newIndex].startsWith(historyState_.prefixFilter_))
        {
            setHistoryIndex(newIndex);
            return;
        }
    }
    if(newIndex >= historyState_.history_.length())
    {
        // reached history end => go back at the orginal edit state
        QString savedFilter = historyState_.prefixFilter_;
        setHistoryIndex(newIndex);
        historyState_.prefixFilter_ = savedFilter;
    }
}

/*!
 * \brief Select an entry from command history and write it in the editor
 * \param index Index of the history entry
 */
void QCommandEdit::setHistoryIndex(int index)
{
    if(index < 0 || index > historyState_.history_.length())
        return;

    ghostSuffix_ = "";

    if(index >= historyState_.history_.length())
    {
        // going past last item resets the editor to whatever text
        // has been entered before beginning history navigation
        setText(historyState_.prefixFilter_);
        historyState_.reset();
        searchMatchingHistoryAndShowGhost();
    }
    else
    {
        historyState_.index_ = index;
        setText(historyState_.history_[index]);
    }

    QTimer::singleShot(0, this, &QCommandEdit::moveCursorToEnd);
    setToolTipAtCursor("");
}

/*!
 * \brief Set the list of completions for the current cursor position
 * \param completion The list of completions
 */
void QCommandEdit::setCompletion(const QStringList &completion)
{
    completionState_.completion_ = completion;

    if(completionState_.requested_)
        navigateCompletion(1);
}

/*!
 * \brief Reset the completion state
 */
void QCommandEdit::resetCompletion()
{
    completionState_.reset();
}

/*!
 * \brief Set a proposed completion by inserting a selected text at the cursor
 * \param s the completion text to insert
 *
 * This will insert a proposed completion at the cursor position.
 * The completion text will be inserted and selected.
 * If there is already some selected text, it will be replaced.
 *
 * The completion insertion point is the cursor position, of the selection
 * start if there is selected text.
 */
void QCommandEdit::setCurrentCompletion(const QString &s)
{
    int c = hasSelectedText() ? selectionStart() : cursorPosition();
    QString before = text().left(c);
    QString after = text().mid(c + selectedText().length());
    QString newText = before + s + after;
    bool oldBlockSignals = blockSignals(true);
    setText(newText);
    setCursorPosition(before.length());
    setSelection(before.length(), s.length());
    blockSignals(oldBlockSignals);
    searchMatchingHistoryAndShowGhost();
}

/*!
 * \brief Navigate thru completion choices
 * \param delta 1 to choose next or -1 to choose previous
 */
void QCommandEdit::navigateCompletion(int delta)
{
    if(delta == 0) return;

    // clip delta to +1/-1:
    delta = (delta < -1 ? -1 : delta > 1 ? 1 : delta);

    // compute actual index
    int newIndex = completionState_.index_;
    newIndex += delta;

    if(newIndex < 0 || newIndex >= completionState_.completion_.length())
        return;

    completionState_.index_ = newIndex;
    setCurrentCompletion(completionState_.completion_[newIndex]);
}

/*!
 * \brief Accept the currently selected completion choice
 */
void QCommandEdit::acceptCompletion()
{
    if(hasSelectedText())
    {
        QString currentCompletion = selectedText();
        cancelCompletion();
        int c = cursorPosition();
        QString t = text();
        setText(t.left(c) + currentCompletion + t.mid(c));
        setCursorPosition(c + currentCompletion.length());
        completionState_.reset();
        searchMatchingHistoryAndShowGhost();
    }
}

/*!
 * \brief Cancel the completion
 */
void QCommandEdit::cancelCompletion()
{
    if(hasSelectedText())
    {
        setCurrentCompletion("");
        completionState_.reset();
    }
}

/*!
 * \brief Display a tooltip at the cursor position
 * \param tip The tooltip text
 */
void QCommandEdit::setToolTipAtCursor(const QString &tip)
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

/*!
 * \brief Move cursor to end of line
 */
void QCommandEdit::moveCursorToEnd()
{
    setCursorPosition(text().size());
}

void QCommandEdit::onReturnPressed()
{
    if(text().isEmpty()) return;

    if(hasSelectedText())
        acceptCompletion();
    else
        emit execute(text());
}

void QCommandEdit::onEscapePressed()
{
    if(text().isEmpty())
        emit escape();
    if(hasSelectedText())
        cancelCompletion();
    else
        clear();
}

void QCommandEdit::onUpPressed()
{
    navigateHistory(-1);
}

void QCommandEdit::onDownPressed()
{
    navigateHistory(1);
}

void QCommandEdit::onTabPressed()
{
    if(completionState_.completion_.isEmpty())
    {
        if(completionState_.requested_)
            return;
        completionState_.requested_ = true;
        emit askCompletion(text(), cursorPosition());
        return;
    }
    navigateCompletion(1);
}

void QCommandEdit::onShiftTabPressed()
{
    navigateCompletion(-1);
}

void QCommandEdit::onSelectionChanged()
{
    completionState_.reset();
}

void QCommandEdit::onCursorPositionChanged(int old, int now)
{
    Q_UNUSED(old);
    Q_UNUSED(now);
    completionState_.reset();
}

void QCommandEdit::onTextEdited()
{
    resetCompletion();
    historyState_.prefixFilter_ = text();

    if(cursorPosition() == text().length())
        searchMatchingHistoryAndShowGhost();
}

void QCommandEdit::searchMatchingHistoryAndShowGhost()
{
    if(!text().isEmpty() && showMatchingHistory_)
    {
        for(int i = historyState_.history_.length() - 1; i >= 0; --i)
        {
            if(historyState_.history_[i].startsWith(text()))
            {
                ghostSuffix_ = historyState_.history_[i].mid(text().length());
                repaint();
                return;
            }
        }
    }

    if(!ghostSuffix_.isEmpty())
    {
        ghostSuffix_ = "";
        repaint();
    }
}

void QCommandEdit::HistoryState::reset()
{
    index_ = -1;
    prefixFilter_ = "";
}

void QCommandEdit::CompletionState::reset()
{
    completion_.clear();
    requested_ = false;
    index_ = -1;
}
