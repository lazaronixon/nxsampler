#pragma once

#include <QList>
#include <QWidget>

#include <bitset>

// A clickable keyboard covering MIDI notes 0–127, with each note number above its key.
// Click toggles a key, click-drag paints the same state over more keys, and
// Shift+click selects every key between the last clicked key and this one.
class KeyboardWidget : public QWidget
{
    Q_OBJECT

public:
    static constexpr int kKeyCount = 128;

    explicit KeyboardWidget(QWidget* parent = nullptr);

    QList<int> selectedKeys() const;
    void setSelectedKeys(const QList<int>& keys);
    void selectAll();
    void clearSelection();

    // Horizontal centre of a key, for scrolling it into view.
    int keyCenterX(int key) const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

    static bool isBlackKey(int key);
    static QString noteName(int key); // middle C (60) is C4

signals:
    void selectionChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QRect keyRect(int key) const;
    int keyAt(const QPoint& pos) const;
    void setKey(int key, bool selected);

    std::bitset<kKeyCount> selection;
    int anchorKey = -1;     // last clicked key, start of a Shift+click range
    int lastDragKey = -1;
    bool dragSelects = true;
    bool dragging = false;
};
