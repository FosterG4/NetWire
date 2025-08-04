#include <QtTest/QtTest>
#include <QSignalSpy>
#include "../../src/charts/bandwidthchart.h"

class TestBandwidthChart : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    
    void testInitialization();
    void testAddDataPoints();
    void testMaxPoints();
    void testClear();
    void testErrorHandling();

private:
    BandwidthChart *m_chart;
};

void TestBandwidthChart::initTestCase()
{
    m_chart = new BandwidthChart();
    QVERIFY(m_chart != nullptr);
}

void TestBandwidthChart::cleanupTestCase()
{
    delete m_chart;
}

void TestBandwidthChart::testInitialization()
{
    // Test default max points
    QCOMPARE(m_chart->maxPoints(), BandwidthChart::DEFAULT_MAX_POINTS);
    
    // Test initial state
    QCOMPARE(m_chart->chart()->title(), QString("Network Bandwidth"));
    QVERIFY(m_chart->chart()->series().count() == 2);
}

void TestBandwidthChart::testAddDataPoints()
{
    // Test adding single data point
    m_chart->addDataPoint(1000, 500);
    
    // Test adding multiple data points
    for (int i = 0; i < 10; ++i) {
        m_chart->addDataPoint(i * 100, i * 50);
    }
    
    // Verify data points were added
    QCOMPARE(m_chart->chart()->series().at(0)->count(), 11);
    QCOMPARE(m_chart->chart()->series().at(1)->count(), 11);
}

void TestBandwidthChart::testMaxPoints()
{
    // Set a small max points for testing
    const int testMax = 5;
    m_chart->setMaxPoints(testMax);
    QCOMPARE(m_chart->maxPoints(), testMax);
    
    // Add more points than max
    for (int i = 0; i < testMax * 2; ++i) {
        m_chart->addDataPoint(i * 100, i * 50);
    }
    
    // Should only keep the most recent points
    QCOMPARE(m_chart->chart()->series().at(0)->count(), testMax);
    QCOMPARE(m_chart->chart()->series().at(1)->count(), testMax);
    
    // Test invalid max points
    m_chart->setMaxPoints(-5);
    QVERIFY(m_chart->maxPoints() >= 10); // Should be clamped to minimum
    
    m_chart->setMaxPoints(10000);
    QVERIFY(m_chart->maxPoints() <= BandwidthChart::MAX_ALLOWED_POINTS);
}

void TestBandwidthChart::testClear()
{
    // Add some data
    for (int i = 0; i < 10; ++i) {
        m_chart->addDataPoint(i * 100, i * 50);
    }
    
    // Clear the chart
    m_chart->clear();
    
    // Verify chart is empty
    QCOMPARE(m_chart->chart()->series().at(0)->count(), 0);
    QCOMPARE(m_chart->chart()->series().at(1)->count(), 0);
}

void TestBandwidthChart::testErrorHandling()
{
    // Test error signal emission
    QSignalSpy errorSpy(m_chart, &BandwidthChart::errorOccurred);
    
    // This will cause an error due to null series
    BandwidthChart *badChart = new BandwidthChart();
    delete badChart->chart(); // Force error condition
    
    // Should not crash and should emit error signal
    badChart->addDataPoint(100, 200);
    QVERIFY(errorSpy.count() > 0);
    
    delete badChart;
}

QTEST_MAIN(TestBandwidthChart)
#include "test_bandwidthchart.moc"
