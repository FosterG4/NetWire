#include "animations.h"
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QSequentialAnimationGroup>
#include <QGraphicsOpacityEffect>
#include <QGraphicsTransform>
#include <QGraphicsScale>
#include <QGraphicsRotation>
#include <QGraphicsColorizeEffect>
#include <QTimer>
#include <QEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QDebug>

QPropertyAnimation* Animations::fadeIn(QWidget *widget, int duration, qreal startOpacity, qreal endOpacity)
{
    setupOpacityEffect(widget);
    
    QPropertyAnimation *animation = new QPropertyAnimation(widget, "opacity");
    animation->setDuration(duration);
    animation->setStartValue(startOpacity);
    animation->setEndValue(endOpacity);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    
    return animation;
}

QPropertyAnimation* Animations::fadeOut(QWidget *widget, int duration, qreal startOpacity, qreal endOpacity)
{
    setupOpacityEffect(widget);
    
    QPropertyAnimation *animation = new QPropertyAnimation(widget, "opacity");
    animation->setDuration(duration);
    animation->setStartValue(startOpacity);
    animation->setEndValue(endOpacity);
    animation->setEasingCurve(QEasingCurve::InCubic);
    
    return animation;
}

QPropertyAnimation* Animations::slideInLeft(QWidget *widget, int duration, int distance)
{
    QPropertyAnimation *animation = new QPropertyAnimation(widget, "geometry");
    animation->setDuration(duration);
    
    QRect startRect = widget->geometry();
    QRect endRect = startRect;
    startRect.translate(-distance, 0);
    
    animation->setStartValue(startRect);
    animation->setEndValue(endRect);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    
    return animation;
}

QPropertyAnimation* Animations::slideInRight(QWidget *widget, int duration, int distance)
{
    QPropertyAnimation *animation = new QPropertyAnimation(widget, "geometry");
    animation->setDuration(duration);
    
    QRect startRect = widget->geometry();
    QRect endRect = startRect;
    startRect.translate(distance, 0);
    
    animation->setStartValue(startRect);
    animation->setEndValue(endRect);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    
    return animation;
}

QPropertyAnimation* Animations::slideInTop(QWidget *widget, int duration, int distance)
{
    QPropertyAnimation *animation = new QPropertyAnimation(widget, "geometry");
    animation->setDuration(duration);
    
    QRect startRect = widget->geometry();
    QRect endRect = startRect;
    startRect.translate(0, -distance);
    
    animation->setStartValue(startRect);
    animation->setEndValue(endRect);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    
    return animation;
}

QPropertyAnimation* Animations::slideInBottom(QWidget *widget, int duration, int distance)
{
    QPropertyAnimation *animation = new QPropertyAnimation(widget, "geometry");
    animation->setDuration(duration);
    
    QRect startRect = widget->geometry();
    QRect endRect = startRect;
    startRect.translate(0, distance);
    
    animation->setStartValue(startRect);
    animation->setEndValue(endRect);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    
    return animation;
}

QPropertyAnimation* Animations::scale(QWidget *widget, int duration, qreal startScale, qreal endScale)
{
    setupTransformEffect(widget);
    
    QPropertyAnimation *animation = new QPropertyAnimation(widget, "scale");
    animation->setDuration(duration);
    animation->setStartValue(startScale);
    animation->setEndValue(endScale);
    animation->setEasingCurve(QEasingCurve::OutBack);
    
    return animation;
}

QPropertyAnimation* Animations::colorTransition(QWidget *widget, int duration, const QColor &startColor, const QColor &endColor)
{
    setupColorEffect(widget);
    
    QPropertyAnimation *animation = new QPropertyAnimation(widget, "color");
    animation->setDuration(duration);
    animation->setStartValue(startColor);
    animation->setEndValue(endColor);
    animation->setEasingCurve(QEasingCurve::InOutQuad);
    
    return animation;
}

QSequentialAnimationGroup* Animations::pulse(QWidget *widget, int duration, qreal scaleFactor)
{
    setupTransformEffect(widget);
    
    QSequentialAnimationGroup *group = new QSequentialAnimationGroup;
    
    // Scale up
    QPropertyAnimation *scaleUp = new QPropertyAnimation(widget, "scale");
    scaleUp->setDuration(duration / 2);
    scaleUp->setStartValue(1.0);
    scaleUp->setEndValue(scaleFactor);
    scaleUp->setEasingCurve(QEasingCurve::OutQuad);
    
    // Scale down
    QPropertyAnimation *scaleDown = new QPropertyAnimation(widget, "scale");
    scaleDown->setDuration(duration / 2);
    scaleDown->setStartValue(scaleFactor);
    scaleDown->setEndValue(1.0);
    scaleDown->setEasingCurve(QEasingCurve::InQuad);
    
    group->addAnimation(scaleUp);
    group->addAnimation(scaleDown);
    
    return group;
}

QSequentialAnimationGroup* Animations::shake(QWidget *widget, int duration, int intensity)
{
    QSequentialAnimationGroup *group = new QSequentialAnimationGroup;
    
    QRect originalRect = widget->geometry();
    int stepDuration = duration / 8;
    
    // Shake sequence: left, right, left, right, left, right, center
    QList<int> movements = {-intensity, intensity, -intensity, intensity, -intensity, intensity, 0};
    
    for (int movement : movements) {
        QPropertyAnimation *shakeStep = new QPropertyAnimation(widget, "geometry");
        shakeStep->setDuration(stepDuration);
        
        QRect startRect = originalRect;
        QRect endRect = originalRect;
        startRect.translate(movement, 0);
        
        shakeStep->setStartValue(startRect);
        shakeStep->setEndValue(endRect);
        shakeStep->setEasingCurve(QEasingCurve::Linear);
        
        group->addAnimation(shakeStep);
    }
    
    return group;
}

QSequentialAnimationGroup* Animations::bounce(QWidget *widget, int duration, int bounceHeight)
{
    QSequentialAnimationGroup *group = new QSequentialAnimationGroup;
    
    QRect originalRect = widget->geometry();
    int stepDuration = duration / 4;
    
    // Bounce sequence: up, down, up, down
    QList<int> movements = {-bounceHeight, bounceHeight, -bounceHeight/2, bounceHeight/2};
    
    for (int movement : movements) {
        QPropertyAnimation *bounceStep = new QPropertyAnimation(widget, "geometry");
        bounceStep->setDuration(stepDuration);
        
        QRect startRect = originalRect;
        QRect endRect = originalRect;
        startRect.translate(0, movement);
        
        bounceStep->setStartValue(startRect);
        bounceStep->setEndValue(endRect);
        bounceStep->setEasingCurve(QEasingCurve::OutBounce);
        
        group->addAnimation(bounceStep);
    }
    
    return group;
}

QPropertyAnimation* Animations::resize(QWidget *widget, int duration, const QSize &startSize, const QSize &endSize)
{
    QPropertyAnimation *animation = new QPropertyAnimation(widget, "geometry");
    animation->setDuration(duration);
    
    QRect startRect = widget->geometry();
    QRect endRect = widget->geometry();
    
    if (!startSize.isNull()) {
        startRect.setSize(startSize);
    }
    if (!endSize.isNull()) {
        endRect.setSize(endSize);
    }
    
    animation->setStartValue(startRect);
    animation->setEndValue(endRect);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    
    return animation;
}

QPropertyAnimation* Animations::move(QWidget *widget, int duration, const QPoint &startPos, const QPoint &endPos)
{
    QPropertyAnimation *animation = new QPropertyAnimation(widget, "geometry");
    animation->setDuration(duration);
    
    QRect startRect = widget->geometry();
    QRect endRect = widget->geometry();
    
    if (!startPos.isNull()) {
        startRect.moveTo(startPos);
    }
    if (!endPos.isNull()) {
        endRect.moveTo(endPos);
    }
    
    animation->setStartValue(startRect);
    animation->setEndValue(endRect);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    
    return animation;
}

QParallelAnimationGroup* Animations::stagger(const QList<QWidget*> &widgets, const QString &animationType, 
                                            int duration, int staggerDelay)
{
    QParallelAnimationGroup *group = new QParallelAnimationGroup;
    
    for (int i = 0; i < widgets.size(); ++i) {
        QWidget *widget = widgets[i];
        QPropertyAnimation *animation = nullptr;
        
        if (animationType == "fadeIn") {
            animation = fadeIn(widget, duration);
        } else if (animationType == "slideInLeft") {
            animation = slideInLeft(widget, duration);
        } else if (animationType == "slideInRight") {
            animation = slideInRight(widget, duration);
        } else if (animationType == "scale") {
            animation = scale(widget, duration);
        }
        
        if (animation) {
            // Add delay for stagger effect
            QTimer::singleShot(i * staggerDelay, [animation]() {
                animation->start();
            });
            
            group->addAnimation(animation);
        }
    }
    
    return group;
}

void Animations::applyHoverEffect(QWidget *widget, const QColor &normalColor, const QColor &hoverColor, int duration)
{
    // Store original palette
    QPalette originalPalette = widget->palette();
    
    // Create hover animation
    QPropertyAnimation *hoverAnimation = new QPropertyAnimation(widget, "palette");
    hoverAnimation->setDuration(duration);
    hoverAnimation->setStartValue(originalPalette);
    
    QPalette hoverPalette = originalPalette;
    hoverPalette.setColor(QPalette::Window, hoverColor);
    hoverAnimation->setEndValue(hoverPalette);
    
    // Connect hover events
    widget->installEventFilter(new QObject(widget));
    widget->setMouseTracking(true);
    
    // Store animation for later use
    widget->setProperty("hoverAnimation", QVariant::fromValue(hoverAnimation));
}

void Animations::applyFocusEffect(QWidget *widget, const QColor &normalColor, const QColor &focusColor, int duration)
{
    // Store original palette
    QPalette originalPalette = widget->palette();
    
    // Create focus animation
    QPropertyAnimation *focusAnimation = new QPropertyAnimation(widget, "palette");
    focusAnimation->setDuration(duration);
    focusAnimation->setStartValue(originalPalette);
    
    QPalette focusPalette = originalPalette;
    focusPalette.setColor(QPalette::Window, focusColor);
    focusAnimation->setEndValue(focusPalette);
    
    // Store animation for later use
    widget->setProperty("focusAnimation", QVariant::fromValue(focusAnimation));
}

QPropertyAnimation* Animations::loadingSpinner(QWidget *widget, int duration)
{
    setupTransformEffect(widget);
    
    QPropertyAnimation *animation = new QPropertyAnimation(widget, "rotation");
    animation->setDuration(duration);
    animation->setStartValue(0);
    animation->setEndValue(360);
    animation->setLoopCount(-1); // Infinite loop
    animation->setEasingCurve(QEasingCurve::Linear);
    
    return animation;
}

QPropertyAnimation* Animations::progressFill(QWidget *widget, int duration, int startValue, int endValue)
{
    QPropertyAnimation *animation = new QPropertyAnimation(widget, "value");
    animation->setDuration(duration);
    animation->setStartValue(startValue);
    animation->setEndValue(endValue);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    
    return animation;
}

void Animations::setupOpacityEffect(QWidget *widget)
{
    if (!widget->graphicsEffect() || !qobject_cast<QGraphicsOpacityEffect*>(widget->graphicsEffect())) {
        QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(widget);
        widget->setGraphicsEffect(effect);
    }
}

void Animations::setupTransformEffect(QWidget *widget)
{
    if (!widget->graphicsEffect() || !qobject_cast<QGraphicsTransform*>(widget->graphicsEffect())) {
        // For scale and rotation, we'll use a custom approach
        // since Qt doesn't have built-in transform effects for widgets
        widget->setProperty("scale", 1.0);
        widget->setProperty("rotation", 0.0);
    }
}

void Animations::setupColorEffect(QWidget *widget)
{
    if (!widget->graphicsEffect() || !qobject_cast<QGraphicsColorizeEffect*>(widget->graphicsEffect())) {
        QGraphicsColorizeEffect *effect = new QGraphicsColorizeEffect(widget);
        widget->setGraphicsEffect(effect);
    }
} 