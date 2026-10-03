#include "KeyboardWidget.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

namespace {

constexpr int kWhiteWidth = 24;
constexpr int kWhiteHeight = 130;
constexpr int kBlackWidth = 15;
constexpr int kBlackHeight = 82;
constexpr int kLabelRowHeight = 15;
constexpr int kLabelArea = kLabelRowHeight * 2; // black key numbers on top, white below
constexpr int kMargin = 4;

const QColor kSelectedWhite(255, 170, 60);
const QColor kSelectedBlack(220, 110, 0);

int whiteIndex(int key)
{
    // Number of white keys before `key`.
    static constexpr int whitesBeforeInOctave[12] = {0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6};
    return (key / 12) * 7 + whitesBeforeInOctave[key % 12];
}

int whiteKeyCount()
{
    int count = 0;
    for (int key = 0; key < KeyboardWidget::kKeyCount; ++key)
        if (!KeyboardWidget::isBlackKey(key))
            ++count;
    return count;
}

} // namespace

KeyboardWidget::KeyboardWidget(QWidget* parent)
: QWidget(parent)
{
    setMouseTracking(false);
    setCursor(Qt::PointingHandCursor);
}

bool KeyboardWidget::isBlackKey(int key)
{
    switch (key % 12)
    {
    case 1: case 3: case 6: case 8: case 10:
        return true;
    default:
        return false;
    }
}

QString KeyboardWidget::noteName(int key)
{
    static const char* names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return QStringLiteral("%1%2").arg(QLatin1String(names[key % 12])).arg(key / 12 - 1);
}

QSize KeyboardWidget::sizeHint() const
{
    return {kMargin * 2 + whiteKeyCount() * kWhiteWidth + 1,
            kMargin * 2 + kLabelArea + kWhiteHeight + 1};
}

QRect KeyboardWidget::keyRect(int key) const
{
    const int top = kMargin + kLabelArea;
    if (!isBlackKey(key))
        return {kMargin + whiteIndex(key) * kWhiteWidth, top, kWhiteWidth, kWhiteHeight};

    // A black key straddles the boundary after the white key below it.
    const int boundary = kMargin + whiteIndex(key) * kWhiteWidth;
    return {boundary - kBlackWidth / 2, top, kBlackWidth, kBlackHeight};
}

int KeyboardWidget::keyCenterX(int key) const
{
    return keyRect(std::clamp(key, 0, kKeyCount - 1)).center().x();
}

int KeyboardWidget::keyAt(const QPoint& pos) const
{
    for (int key = 0; key < kKeyCount; ++key)
        if (isBlackKey(key) && keyRect(key).contains(pos))
            return key;
    for (int key = 0; key < kKeyCount; ++key)
        if (!isBlackKey(key) && keyRect(key).contains(pos))
            return key;
    return -1;
}

void KeyboardWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    QFont numberFont = font();
    numberFont.setPointSizeF(8.5);
    QFont nameFont = font();
    nameFont.setPointSizeF(8);

    const QColor labelColor = palette().color(QPalette::WindowText);

    // White keys
    for (int key = 0; key < kKeyCount; ++key)
    {
        if (isBlackKey(key))
            continue;
        const QRect r = keyRect(key);
        painter.setPen(QColor(60, 60, 60));
        painter.setBrush(selection[static_cast<size_t>(key)] ? kSelectedWhite : QColor(250, 250, 250));
        painter.drawRect(r);

        if (key % 12 == 0)
        {
            painter.setFont(nameFont);
            painter.setPen(key == 60 ? QColor(200, 0, 0) : QColor(90, 90, 90));
            painter.drawText(QRect(r.left(), r.bottom() - 18, r.width(), 16), Qt::AlignCenter,
                             noteName(key));
        }

        painter.setFont(numberFont);
        painter.setPen(labelColor);
        painter.drawText(QRect(r.left() - 4, kMargin + kLabelRowHeight, r.width() + 8, kLabelRowHeight),
                         Qt::AlignCenter, QString::number(key));
    }

    // Black keys on top
    for (int key = 0; key < kKeyCount; ++key)
    {
        if (!isBlackKey(key))
            continue;
        const QRect r = keyRect(key);
        painter.setPen(QColor(20, 20, 20));
        painter.setBrush(selection[static_cast<size_t>(key)] ? kSelectedBlack : QColor(30, 30, 30));
        painter.drawRect(r);

        painter.setFont(numberFont);
        painter.setPen(labelColor);
        painter.drawText(QRect(r.center().x() - 14, kMargin, 28, kLabelRowHeight), Qt::AlignCenter,
                         QString::number(key));
    }
}

void KeyboardWidget::setKey(int key, bool selected)
{
    if (key < 0 || key >= kKeyCount || selection[static_cast<size_t>(key)] == selected)
        return;
    selection[static_cast<size_t>(key)] = selected;
    update(keyRect(key).adjusted(-kBlackWidth, 0, kBlackWidth, 0));
    emit selectionChanged();
}

void KeyboardWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;
    const int key = keyAt(event->position().toPoint());
    if (key < 0)
        return;

    if ((event->modifiers() & Qt::ShiftModifier) && anchorKey >= 0)
    {
        const int low = std::min(anchorKey, key);
        const int high = std::max(anchorKey, key);
        for (int k = low; k <= high; ++k)
            selection[static_cast<size_t>(k)] = true;
        update();
        emit selectionChanged();
        anchorKey = key;
        return;
    }

    dragSelects = !selection[static_cast<size_t>(key)];
    dragging = true;
    lastDragKey = key;
    anchorKey = key;
    setKey(key, dragSelects);
}

void KeyboardWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!dragging)
        return;
    const int key = keyAt(event->position().toPoint());
    if (key < 0 || key == lastDragKey)
        return;
    lastDragKey = key;
    setKey(key, dragSelects);
}

void KeyboardWidget::mouseReleaseEvent(QMouseEvent*)
{
    dragging = false;
    lastDragKey = -1;
}

QList<int> KeyboardWidget::selectedKeys() const
{
    QList<int> keys;
    for (int key = 0; key < kKeyCount; ++key)
        if (selection[static_cast<size_t>(key)])
            keys.append(key);
    return keys;
}

void KeyboardWidget::setSelectedKeys(const QList<int>& keys)
{
    selection.reset();
    for (int key : keys)
        if (key >= 0 && key < kKeyCount)
            selection[static_cast<size_t>(key)] = true;
    update();
    emit selectionChanged();
}

void KeyboardWidget::selectAll()
{
    selection.set();
    update();
    emit selectionChanged();
}

void KeyboardWidget::clearSelection()
{
    selection.reset();
    update();
    emit selectionChanged();
}
