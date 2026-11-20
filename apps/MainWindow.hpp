#pragma once
#include <QMainWindow>
#include <QKeyEvent>
#include <memory>
#include <vector>
#include <string>
#include "image_canvas.hpp"
#include "enhancement_pipeline.hpp"
#include "dataset_manager.hpp"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onOpenImage();
    void onOpenFolder();
    void onLoadReference();
    void onSetDatasetRoot();
    void onDatasetSelected(int index);
    void onFileSelected(int index);
    void onPrevImage();
    void onNextImage();
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
    void initializeDatasets();
    void refreshDatasets(const QString& rootPath = "");
    void loadDataset(int datasetIdx);
    void loadImage(const QString& path);
    void loadReference(const QString& path);
    void updateProcessing();
    void updateMetricsDisplay();

    static QString locateSampleDirectory();
    static QImage matToQImage(const cv::Mat& mat);

    std::unique_ptr<Ui::MainWindow> ui;

    // Dataset navigation
    QString m_datasetRoot;
    std::vector<ir_enhance::DatasetEntry> m_datasets;
    std::vector<std::string> m_currentFiles;
    int m_currentFileIdx = -1;

    // Current data
    cv::Mat m_inputMat;
    cv::Mat m_refMat;
    QString m_currentImagePath;
    QString m_sampleDir;
    ir_enhance::IntermediateStages m_stages;
    bool m_isUpdatingUI = false;
};
