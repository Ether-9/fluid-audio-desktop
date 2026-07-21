from PySide6.QtWidgets import QStackedWidget, QGraphicsOpacityEffect
from PySide6.QtCore import QPropertyAnimation, QEasingCurve, QPoint, QParallelAnimationGroup

class FadingSlidingStackedWidget(QStackedWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.duration = 350
        self.easing = QEasingCurve.Type.OutCubic
        self.animation_group = None

    def setCurrentIndex(self, index: int):
        if self.currentIndex() == index:
            return
        
        # If an animation is already running, stop it clean
        if self.animation_group and self.animation_group.state() == QParallelAnimationGroup.State.Running:
            self.animation_group.stop()

        current_widget = self.currentWidget()
        next_widget = self.widget(index)
        
        if not current_widget or not next_widget:
            super().setCurrentIndex(index)
            return

        # Prepare dimensions
        width = self.width()
        
        # Determine direction (forward vs backward)
        is_forward = index > self.currentIndex()
        start_offset = width if is_forward else -width

        # Position target widget correctly before showing
        next_widget.setGeometry(0, 0, width, self.height())
        next_widget.show()
        next_widget.raise_()

        # Set up opacity effects for premium cross-fade
        opacity_current = QGraphicsOpacityEffect(current_widget)
        opacity_next = QGraphicsOpacityEffect(next_widget)
        current_widget.setGraphicsEffect(opacity_current)
        next_widget.setGraphicsEffect(opacity_next)

        # Slide Animations
        anim_current_pos = QPropertyAnimation(current_widget, b"pos")
        anim_current_pos.setDuration(self.duration)
        anim_current_pos.setEasingCurve(self.easing)
        anim_current_pos.setStartValue(QPoint(0, 0))
        anim_current_pos.setEndValue(QPoint(-start_offset, 0))

        anim_next_pos = QPropertyAnimation(next_widget, b"pos")
        anim_next_pos.setDuration(self.duration)
        anim_next_pos.setEasingCurve(self.easing)
        anim_next_pos.setStartValue(QPoint(start_offset, 0))
        anim_next_pos.setEndValue(QPoint(0, 0))

        # Fade Animations
        anim_current_fade = QPropertyAnimation(opacity_current, b"opacity")
        anim_current_fade.setDuration(self.duration)
        anim_current_fade.setStartValue(1.0)
        anim_current_fade.setEndValue(0.0)

        anim_next_fade = QPropertyAnimation(opacity_next, b"opacity")
        anim_next_fade.setDuration(self.duration)
        anim_next_fade.setStartValue(0.0)
        anim_next_fade.setEndValue(1.0)

        # Group and play animations simultaneously
        self.animation_group = QParallelAnimationGroup(self)
        self.animation_group.addAnimation(anim_current_pos)
        self.animation_group.addAnimation(anim_next_pos)
        self.animation_group.addAnimation(anim_current_fade)
        self.animation_group.addAnimation(anim_next_fade)

        def on_finished():
            # Reset effects to free system resources
            current_widget.setGraphicsEffect(None)
            next_widget.setGraphicsEffect(None)
            super(FadingSlidingStackedWidget, self).setCurrentIndex(index)

        self.animation_group.finished.connect(on_finished)
        self.animation_group.start()