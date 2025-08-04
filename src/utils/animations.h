#ifndef ANIMATIONS_H
#define ANIMATIONS_H

#include <QObject>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QSequentialAnimationGroup>
#include <QGraphicsOpacityEffect>
#include <QWidget>
#include <QTimer>
#include <QColor>

/**
 * @brief The Animations class provides utility functions for creating smooth UI animations.
 * 
 * This class contains static methods for common animation patterns used throughout
 * the Netwire application, including fade effects, slide animations, and color transitions.
 */
class Animations : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Creates a fade-in animation for a widget.
     * @param widget The widget to animate.
     * @param duration Animation duration in milliseconds.
     * @param startOpacity Starting opacity (0.0 to 1.0).
     * @param endOpacity Ending opacity (0.0 to 1.0).
     * @return The animation object.
     */
    static QPropertyAnimation* fadeIn(QWidget *widget, int duration = 300, 
                                     qreal startOpacity = 0.0, qreal endOpacity = 1.0);

    /**
     * @brief Creates a fade-out animation for a widget.
     * @param widget The widget to animate.
     * @param duration Animation duration in milliseconds.
     * @param startOpacity Starting opacity (0.0 to 1.0).
     * @param endOpacity Ending opacity (0.0 to 1.0).
     * @return The animation object.
     */
    static QPropertyAnimation* fadeOut(QWidget *widget, int duration = 300, 
                                      qreal startOpacity = 1.0, qreal endOpacity = 0.0);

    /**
     * @brief Creates a slide-in animation from the left.
     * @param widget The widget to animate.
     * @param duration Animation duration in milliseconds.
     * @param distance Distance to slide in pixels.
     * @return The animation object.
     */
    static QPropertyAnimation* slideInLeft(QWidget *widget, int duration = 300, int distance = 100);

    /**
     * @brief Creates a slide-in animation from the right.
     * @param widget The widget to animate.
     * @param duration Animation duration in milliseconds.
     * @param distance Distance to slide in pixels.
     * @return The animation object.
     */
    static QPropertyAnimation* slideInRight(QWidget *widget, int duration = 300, int distance = 100);

    /**
     * @brief Creates a slide-in animation from the top.
     * @param widget The widget to animate.
     * @param duration Animation duration in milliseconds.
     * @param distance Distance to slide in pixels.
     * @return The animation object.
     */
    static QPropertyAnimation* slideInTop(QWidget *widget, int duration = 300, int distance = 100);

    /**
     * @brief Creates a slide-in animation from the bottom.
     * @param widget The widget to animate.
     * @param duration Animation duration in milliseconds.
     * @param distance Distance to slide in pixels.
     * @return The animation object.
     */
    static QPropertyAnimation* slideInBottom(QWidget *widget, int duration = 300, int distance = 100);

    /**
     * @brief Creates a scale animation for a widget.
     * @param widget The widget to animate.
     * @param duration Animation duration in milliseconds.
     * @param startScale Starting scale factor.
     * @param endScale Ending scale factor.
     * @return The animation object.
     */
    static QPropertyAnimation* scale(QWidget *widget, int duration = 300, 
                                    qreal startScale = 0.8, qreal endScale = 1.0);

    /**
     * @brief Creates a color transition animation for a widget's background.
     * @param widget The widget to animate.
     * @param duration Animation duration in milliseconds.
     * @param startColor Starting color.
     * @param endColor Ending color.
     * @return The animation object.
     */
    static QPropertyAnimation* colorTransition(QWidget *widget, int duration = 300,
                                              const QColor &startColor = QColor(),
                                              const QColor &endColor = QColor());

    /**
     * @brief Creates a pulse animation (scale up and down).
     * @param widget The widget to animate.
     * @param duration Animation duration in milliseconds.
     * @param scaleFactor Maximum scale factor.
     * @return The animation group containing the pulse sequence.
     */
    static QSequentialAnimationGroup* pulse(QWidget *widget, int duration = 600, qreal scaleFactor = 1.1);

    /**
     * @brief Creates a shake animation for error feedback.
     * @param widget The widget to animate.
     * @param duration Animation duration in milliseconds.
     * @param intensity Shake intensity in pixels.
     * @return The animation group containing the shake sequence.
     */
    static QSequentialAnimationGroup* shake(QWidget *widget, int duration = 500, int intensity = 10);

    /**
     * @brief Creates a bounce animation for success feedback.
     * @param widget The widget to animate.
     * @param duration Animation duration in milliseconds.
     * @param bounceHeight Bounce height in pixels.
     * @return The animation group containing the bounce sequence.
     */
    static QSequentialAnimationGroup* bounce(QWidget *widget, int duration = 800, int bounceHeight = 20);

    /**
     * @brief Creates a smooth window resize animation.
     * @param widget The widget to animate.
     * @param duration Animation duration in milliseconds.
     * @param startSize Starting size.
     * @param endSize Ending size.
     * @return The animation object.
     */
    static QPropertyAnimation* resize(QWidget *widget, int duration = 300,
                                     const QSize &startSize = QSize(),
                                     const QSize &endSize = QSize());

    /**
     * @brief Creates a smooth window move animation.
     * @param widget The widget to animate.
     * @param duration Animation duration in milliseconds.
     * @param startPos Starting position.
     * @param endPos Ending position.
     * @return The animation object.
     */
    static QPropertyAnimation* move(QWidget *widget, int duration = 300,
                                   const QPoint &startPos = QPoint(),
                                   const QPoint &endPos = QPoint());

    /**
     * @brief Creates a staggered animation for multiple widgets.
     * @param widgets List of widgets to animate.
     * @param animationType Type of animation to apply.
     * @param duration Animation duration in milliseconds.
     * @param staggerDelay Delay between each widget animation in milliseconds.
     * @return The animation group containing all animations.
     */
    static QParallelAnimationGroup* stagger(const QList<QWidget*> &widgets,
                                           const QString &animationType = "fadeIn",
                                           int duration = 300, int staggerDelay = 50);

    /**
     * @brief Applies a hover effect with smooth color transition.
     * @param widget The widget to apply the effect to.
     * @param normalColor Normal state color.
     * @param hoverColor Hover state color.
     * @param duration Animation duration in milliseconds.
     */
    static void applyHoverEffect(QWidget *widget, const QColor &normalColor, 
                                const QColor &hoverColor, int duration = 200);

    /**
     * @brief Applies a focus effect with smooth color transition.
     * @param widget The widget to apply the effect to.
     * @param normalColor Normal state color.
     * @param focusColor Focus state color.
     * @param duration Animation duration in milliseconds.
     */
    static void applyFocusEffect(QWidget *widget, const QColor &normalColor, 
                                const QColor &focusColor, int duration = 200);

    /**
     * @brief Creates a loading spinner animation.
     * @param widget The widget to animate.
     * @param duration Animation duration in milliseconds.
     * @return The animation object.
     */
    static QPropertyAnimation* loadingSpinner(QWidget *widget, int duration = 1000);

    /**
     * @brief Creates a progress bar fill animation.
     * @param widget The widget to animate.
     * @param duration Animation duration in milliseconds.
     * @param startValue Starting progress value (0-100).
     * @param endValue Ending progress value (0-100).
     * @return The animation object.
     */
    static QPropertyAnimation* progressFill(QWidget *widget, int duration = 500,
                                           int startValue = 0, int endValue = 100);

private:
    static void setupOpacityEffect(QWidget *widget);
    static void setupTransformEffect(QWidget *widget);
    static void setupColorEffect(QWidget *widget);
};

#endif // ANIMATIONS_H 