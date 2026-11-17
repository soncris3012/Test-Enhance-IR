#pragma once
#include <QMainWindow>
#include <memory>
#include "image_canvas.hpp"
#include "enhancement_pipeline.hpp"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onOpenImage();
    void onLoadReference();
    void onSampleSelected(int index);
    void onModeChanged();
    void onParamChanged();
    void onRunAlgorithm();
    void onResetDefaults();
    void onSaveOutput();
    void onExportStages();
    void onSyncViewChanged(double scale, const QPointF& offset);
    void onSyncViewReset();

private:
    void setupConnections();
    void populateSampleList();
    void loadImage(const QString& path);
    void loadReference(const QString& path);
    void updateProcessing();
    void updateMetricsDisplay();
    void updateHeatmap();

    static QString locateSampleDirectory();
    static QImage matToQImage(const cv::Mat& mat);

    std::unique_ptr<Ui::MainWindow> ui;

    // Current data
    cv::Mat m_inputMat;
    cv::Mat m_refMat;
    QString m_currentImagePath;
    QString m_sampleDir;
    ir_enhance::IntermediateStages m_stages;
    bool m_isUpdatingUI = false;
};
