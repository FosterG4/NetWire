#ifndef ANIMATEDCHARTVIEW_H
#define ANIMATEDCHARTVIEW_H

#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QPropertyAnimation>
#include <QTimer>
#include <QEvent>

QT_CHARTS_USE_NAMESPACE

/**
 * @brief The AnimatedChartView class provides a QChartView with enhanced animations and transitions.
 * 
 * This class extends QChartView to add smooth animations for data updates, chart transitions,
 * and interactive feedback. It includes fade effects, scale animations, and smooth data transitions.
 */
class AnimatedChartView : public QChartView
{
    Q_OBJECT

public:
    /**
     * @brief Constructs an AnimatedChartView with the given parent.
     * @param parent The parent widget.
     */
    explicit AnimatedChartView(QWidget *parent = nullptr);

    /**
     * @brief Constructs an AnimatedChartView with the given chart and parent.
     * @param chart The chart to display.
     * @param parent The parent widget.
     */
    explicit AnimatedChartView(QChart *chart, QWidget *parent = nullptr);

    ~AnimatedChartView() override;

public slots:
    /**
     * @brief Animates the chart appearance with a fade-in effect.
     * @param duration Animation duration in milliseconds.
     */
    void animateAppearance(int duration = 500);

    /**
     * @brief Animates the chart disappearance with a fade-out effect.
     * @param duration Animation duration in milliseconds.
     */
    void animateDisappearance(int duration = 300);

    /**
     * @brief Animates data updates with a smooth transition.
     * @param duration Animation duration in milliseconds.
     */
    void animateDataUpdate(int duration = 200);

    /**
     * @brief Animates chart theme changes.
     * @param duration Animation duration in milliseconds.
     */
    void animateThemeChange(int duration = 400);

    /**
     * @brief Animates chart zoom with smooth scaling.
     * @param factor Zoom factor.
     * @param duration Animation duration in milliseconds.
     */
    void animateZoom(qreal factor, int duration = 300);

    /**
     * @brief Animates chart pan with smooth movement.
     * @param delta Movement delta.
     * @param duration Animation duration in milliseconds.
     */
    void animatePan(const QPointF &delta, int duration = 200);

    /**
     * @brief Animates chart reset to default view.
     * @param duration Animation duration in milliseconds.
     */
    void animateReset(int duration = 400);

    /**
     * @brief Animates chart rotation.
     * @param angle Rotation angle in degrees.
     * @param duration Animation duration in milliseconds.
     */
    void animateRotation(qreal angle, int duration = 500);

    /**
     * @brief Animates chart highlight with a pulse effect.
     * @param duration Animation duration in milliseconds.
     */
    void animateHighlight(int duration = 600);

    /**
     * @brief Animates chart error state with a shake effect.
     * @param duration Animation duration in milliseconds.
     */
    void animateError(int duration = 500);

    /**
     * @brief Animates chart success state with a bounce effect.
     * @param duration Animation duration in milliseconds.
     */
    void animateSuccess(int duration = 800);

    /**
     * @brief Animates chart loading state with a spinner effect.
     * @param duration Animation duration in milliseconds.
     */
    void animateLoading(int duration = 1000);

    /**
     * @brief Animates chart data refresh with a smooth transition.
     * @param duration Animation duration in milliseconds.
     */
    void animateRefresh(int duration = 300);

signals:
    /**
     * @brief Emitted when an animation completes.
     * @param animationType Type of animation that completed.
     */
    void animationCompleted(const QString &animationType);

    /**
     * @brief Emitted when chart interaction begins.
     */
    void interactionStarted();

    /**
     * @brief Emitted when chart interaction ends.
     */
    void interactionEnded();

protected:
    /**
     * @brief Override to handle mouse press events with animation feedback.
     */
    void mousePressEvent(QMouseEvent *event) override;

    /**
     * @brief Override to handle mouse release events with animation feedback.
     */
    void mouseReleaseEvent(QMouseEvent *event) override;

    /**
     * @brief Override to handle mouse move events with animation feedback.
     */
    void mouseMoveEvent(QMouseEvent *event) override;

    /**
     * @brief Override to handle wheel events with smooth zoom animation.
     */
    void wheelEvent(QWheelEvent *event) override;

    /**
     * @brief Override to handle resize events with smooth transition.
     */
    void resizeEvent(QResizeEvent *event) override;

    /**
     * @brief Override to handle show events with appearance animation.
     */
    void showEvent(QShowEvent *event) override;

    /**
     * @brief Override to handle hide events with disappearance animation.
     */
    void hideEvent(QHideEvent *event) override;

private slots:
    void onAnimationFinished();
    void onDataUpdateTimer();

private:
    void setupAnimations();
    void setupEventHandling();
    void applyChartAnimation(QPropertyAnimation *animation);
    void applyViewAnimation(QPropertyAnimation *animation);
    void startLoadingAnimation();
    void stopLoadingAnimation();

    QPropertyAnimation *m_appearanceAnimation;
    QPropertyAnimation *m_disappearanceAnimation;
    QPropertyAnimation *m_dataUpdateAnimation;
    QPropertyAnimation *m_themeChangeAnimation;
    QPropertyAnimation *m_zoomAnimation;
    QPropertyAnimation *m_panAnimation;
    QPropertyAnimation *m_resetAnimation;
    QPropertyAnimation *m_rotationAnimation;
    QPropertyAnimation *m_highlightAnimation;
    QPropertyAnimation *m_errorAnimation;
    QPropertyAnimation *m_successAnimation;
    QPropertyAnimation *m_loadingAnimation;
    QPropertyAnimation *m_refreshAnimation;

    QTimer *m_dataUpdateTimer;
    QTimer *m_loadingTimer;
    
    bool m_isAnimating;
    bool m_isLoading;
    bool m_isInteractive;
    
    qreal m_currentZoom;
    qreal m_currentRotation;
    QPointF m_lastPanPosition;
    
    static const int DEFAULT_ANIMATION_DURATION = 300;
    static const int LOADING_ANIMATION_DURATION = 1000;
    static const int DATA_UPDATE_DELAY = 100;
};

#endif // ANIMATEDCHARTVIEW_H 