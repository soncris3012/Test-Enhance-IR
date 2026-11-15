#include "MainWindow.hpp"
#include "ui_MainWindow.h"
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QStatusBar>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), ui(std::make_unique<Ui::MainWindow>()) {
    ui->setupUi(this);

    // Modern Dark Theme Stylesheet
    setStyleSheet(R"(
        QMainWindow { background-color: #1e1e24; }
        QWidget { color: #e0e0e0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; font-size: 12px; }
        QGroupBox { 
            font-weight: bold; 
            border: 1px solid #3d3d4a; 
            border-radius: 6px; 
            margin-top: 10px; 
            padding-top: 14px; 
            background-color: #26262e;
        }
        QGroupBox::title { 
            subcontrol-origin: margin; 
            subcontrol-position: top left; 
            padding: 0 5px; 
            color: #4da6ff; 
        }
        QPushButton { 
            background-color: #3b3b48; 
            border: 1px solid #525263; 
            border-radius: 4px; 
            padding: 6px 12px; 
            font-weight: 500;
        }
        QPushButton:hover { background-color: #4b4b5a; border-color: #4da6ff; }
        QPushButton:pressed { background-color: #2e2e38; }
        QPushButton#btnRun { 
            background-color: #007acc; 
            border: 1px solid #0098ff; 
            font-weight: bold; 
            color: white; 
        }
        QPushButton#btnRun:hover { background-color: #008be5; }
        QComboBox, QSpinBox, QDoubleSpinBox { 
            background-color: #1a1a20; 
            border: 1px solid #444452; 
            border-radius: 4px; 
            padding: 4px 6px; 
            color: #f0f0f0; 
        }
        QSlider::groove:horizontal { 
            height: 4px; 
            background: #3a3a46; 
            border-radius: 2px; 
        }
        QSlider::sub-page:horizontal { 
            background: #4da6ff; 
            border-radius: 2px; 
        }
        QSlider::handle:horizontal { 
            background: #ffffff; 
            border: 1px solid #4da6ff; 
            width: 14px; 
            margin-top: -5px; 
            margin-bottom: -5px; 
            border-radius: 7px; 
        }
        QTabWidget::pane { border: 1px solid #3d3d4a; background-color: #1a1a20; }
        QTabBar::tab { 
            background: #2b2b36; 
            color: #bbb; 
            padding: 8px 16px; 
            border-top-left-radius: 4px; 
            border-top-right-radius: 4px; 
            margin-right: 2px;
        }
        QTabBar::tab:selected { 
            background: #1a1a20; 
            color: #4da6ff; 
            border-top: 2px solid #4da6ff; 
            font-weight: bold;
        }
        QLabel { color: #cccccc; }
    )");

    // Titles for image canvases
    ui->canvasOriginal->setTitle("Original Infrared Input");
    ui->canvasEnhanced->setTitle("Enhanced Output (Proposed Algorithm)");
    ui->canvasStageInput->setTitle("1. Original Image (I)");
    ui->canvasStageBase->setTitle("2. Base Layer (Guided Filter I_base)");
    ui->canvasStageRawDetail->setTitle("3. Raw Detail Layer (I - I_base)");
    ui->canvasStageDenoised->setTitle("4. Denoised Detail (NLM)");
    ui->canvasStageDualGamma->setTitle("5. Dual-Gamma Detail (Eq 8)");
    ui->canvasStageOutput->setTitle("6. Final Fusion (Eq 9)");
    ui->canvasRefTarget->setTitle("Paper's Result Image");
    ui->canvasRefOurs->setTitle("Our Implementation Result");
    ui->canvasRefDiff->setTitle("Difference Heatmap (|Ours - Paper|)");

    // Populate comboNorm
    ui->comboNorm->addItem("Min-Max (Paper Eq)", static_cast<int>(ir_enhance::DetailNormMode::MinMax));
    ui->comboNorm->addItem("Shift (+0.5)", static_cast<int>(ir_enhance::DetailNormMode::Shift05));
    ui->comboNorm->addItem("Symmetric Centered", static_cast<int>(ir_enhance::DetailNormMode::Symmetric));

    // Dynamically find sample_images directory
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates = {
        appDir + "/../sample_images",
        appDir + "/sample_images",
        "/Users/soncris/NCKH2627/infrared_image_enhancement/sample_images",
        "/Users/soncris/nckh2026/infrared_image_enhancement/sample_images"
    };
    for (const QString& candidate : candidates) {
        if (QDir(candidate).exists()) {
            m_sampleDir = QDir(candidate).canonicalPath();
            break;
        }
    }
    if (m_sampleDir.isEmpty()) {
        m_sampleDir = "/Users/soncris/NCKH2627/infrared_image_enhancement/sample_images";
    }

    setupConnections();
    populateSampleList();

    // Load initial default sample
    if (ui->comboSamples->count() > 1) {
        ui->comboSamples->setCurrentIndex(1); // Select Scene 1
    }
}

MainWindow::~MainWindow() = default;

void MainWindow::setupConnections() {
    // Radius (int)
    connect(ui->sliderRadius, &QSlider::valueChanged, this, [this](int val) {
        if (!m_isUpdatingUI) {
            m_isUpdatingUI = true;
            ui->spinRadius->setValue(val);
            m_isUpdatingUI = false;
            onParamChanged();
        }
    });
    connect(ui->spinRadius, &QSpinBox::valueChanged, this, [this](int val) {
        if (!m_isUpdatingUI) {
            m_isUpdatingUI = true;
            ui->sliderRadius->setValue(val);
            m_isUpdatingUI = false;
            onParamChanged();
        }
    });

    // Eps (double scale = 0.001)
    connect(ui->sliderEps, &QSlider::valueChanged, this, [this](int val) {
        if (!m_isUpdatingUI) {
            m_isUpdatingUI = true;
            ui->spinEps->setValue(val * 0.001);
            m_isUpdatingUI = false;
            onParamChanged();
        }
    });
    connect(ui->spinEps, &QDoubleSpinBox::valueChanged, this, [this](double val) {
        if (!m_isUpdatingUI) {
            m_isUpdatingUI = true;
            ui->sliderEps->setValue(static_cast<int>(std::round(val / 0.001)));
            m_isUpdatingUI = false;
            onParamChanged();
        }
    });

    // NLM H (double scale = 0.1)
    connect(ui->sliderH, &QSlider::valueChanged, this, [this](int val) {
        if (!m_isUpdatingUI) {
            m_isUpdatingUI = true;
            ui->spinH->setValue(val * 0.1);
            m_isUpdatingUI = false;
            onParamChanged();
        }
    });
    connect(ui->spinH, &QDoubleSpinBox::valueChanged, this, [this](double val) {
        if (!m_isUpdatingUI) {
            m_isUpdatingUI = true;
            ui->sliderH->setValue(static_cast<int>(std::round(val / 0.1)));
            m_isUpdatingUI = false;
            onParamChanged();
        }
    });

    // Gamma (double scale = 0.1)
    connect(ui->sliderGamma, &QSlider::valueChanged, this, [this](int val) {
        if (!m_isUpdatingUI) {
            m_isUpdatingUI = true;
            ui->spinGamma->setValue(val * 0.1);
            m_isUpdatingUI = false;
            onParamChanged();
        }
    });
    connect(ui->spinGamma, &QDoubleSpinBox::valueChanged, this, [this](double val) {
        if (!m_isUpdatingUI) {
            m_isUpdatingUI = true;
            ui->sliderGamma->setValue(static_cast<int>(std::round(val / 0.1)));
            m_isUpdatingUI = false;
            onParamChanged();
        }
    });

    // Alpha (double scale = 0.01)
    connect(ui->sliderAlpha, &QSlider::valueChanged, this, [this](int val) {
        if (!m_isUpdatingUI) {
            m_isUpdatingUI = true;
            ui->spinAlpha->setValue(val * 0.01);
            m_isUpdatingUI = false;
            onParamChanged();
        }
    });
    connect(ui->spinAlpha, &QDoubleSpinBox::valueChanged, this, [this](double val) {
        if (!m_isUpdatingUI) {
            m_isUpdatingUI = true;
            ui->sliderAlpha->setValue(static_cast<int>(std::round(val / 0.01)));
            m_isUpdatingUI = false;
            onParamChanged();
        }
    });

    // Other inputs
    connect(ui->btnOpen, &QPushButton::clicked, this, &MainWindow::onOpenImage);
    connect(ui->btnLoadRef, &QPushButton::clicked, this, &MainWindow::onLoadReference);
    connect(ui->comboSamples, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onSampleSelected);
    connect(ui->comboNorm, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onParamChanged);
    connect(ui->btnRun, &QPushButton::clicked, this, &MainWindow::onRunAlgorithm);
    connect(ui->btnReset, &QPushButton::clicked, this, &MainWindow::onResetDefaults);
    connect(ui->btnSave, &QPushButton::clicked, this, &MainWindow::onSaveOutput);
    connect(ui->btnExportStages, &QPushButton::clicked, this, &MainWindow::onExportStages);

    // Sync views
    connect(ui->canvasOriginal, &ImageCanvas::viewChanged, this, &MainWindow::onSyncViewChanged);
    connect(ui->canvasEnhanced, &ImageCanvas::viewChanged, this, &MainWindow::onSyncViewChanged);
}

void MainWindow::populateSampleList() {
    ui->comboSamples->clear();
    ui->comboSamples->addItem("-- Select Sample --", "");

    QDir dir(m_sampleDir);
    if (!dir.exists()) return;

    QStringList filters;
    filters << "scene_*_original.png" << "fig2_a_input.png";
    QStringList files = dir.entryList(filters, QDir::Files, QDir::Name);

    for (const QString& f : files) {
        QString label = f;
        if (f.startsWith("scene_")) {
            QString num = f.split("_").value(1);
            label = QString("Scene %1 (From Paper)").arg(num);
        } else if (f == "fig2_a_input.png") {
            label = "Figure 2 Input (3 stacked scenes)";
        }
        ui->comboSamples->addItem(label, dir.absoluteFilePath(f));
    }
}

void MainWindow::onSampleSelected(int index) {
    if (index <= 0) return;
    QString path = ui->comboSamples->itemData(index).toString();
    if (!path.isEmpty()) {
        loadImage(path);

        QFileInfo fi(path);
        QString baseName = fi.baseName();
        if (baseName.startsWith("scene_") && baseName.endsWith("_original")) {
            QString refName = baseName;
            refName.replace("_original", "_paper_ours.png");
            QString refPath = fi.dir().filePath(refName);
            if (QFile::exists(refPath)) {
                loadReference(refPath);
            } else {
                loadReference("");
            }
        } else if (baseName == "fig2_a_input") {
            QString refPath = fi.dir().filePath("fig2_c_output.png");
            if (QFile::exists(refPath)) {
                loadReference(refPath);
            } else {
                loadReference("");
            }
        } else {
            loadReference("");
        }
    }
}

void MainWindow::onOpenImage() {
    QString path = QFileDialog::getOpenFileName(this, "Open Infrared Image", m_sampleDir,
                                              "Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;All Files (*)");
    if (!path.isEmpty()) {
        loadImage(path);
        loadReference("");
    }
}

void MainWindow::onLoadReference() {
    QString path = QFileDialog::getOpenFileName(this, "Open Reference/Ground Truth Image", m_sampleDir,
                                              "Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;All Files (*)");
    if (!path.isEmpty()) {
        loadReference(path);
        updateMetricsDisplay();
        updateHeatmap();
    }
}

void MainWindow::loadImage(const QString& path) {
    cv::Mat img = cv::imread(path.toStdString(), cv::IMREAD_GRAYSCALE);
    if (img.empty()) {
        QMessageBox::warning(this, "Error", "Failed to load image: " + path);
        return;
    }
    m_inputMat = img;
    m_currentImagePath = path;

    QImage qIn = matToQImage(m_inputMat);
    ui->canvasOriginal->setImage(qIn);
    ui->canvasStageInput->setImage(qIn);

    statusBar()->showMessage(QString("Loaded: %1 (%2x%3)").arg(QFileInfo(path).fileName()).arg(img.cols).arg(img.rows));
    updateProcessing();
}

void MainWindow::loadReference(const QString& path) {
    if (path.isEmpty()) {
        m_refMat = cv::Mat();
        ui->lblRefStatus->setText("Reference: None");
        ui->lblPsnr->setText("-");
        ui->lblSsim->setText("-");
        ui->canvasRefTarget->setImage(QImage());
        ui->canvasRefDiff->setImage(QImage());
        return;
    }

    cv::Mat ref = cv::imread(path.toStdString(), cv::IMREAD_GRAYSCALE);
    if (ref.empty()) {
        ui->lblRefStatus->setText("Reference: Failed to load");
        return;
    }
    m_refMat = ref;
    ui->lblRefStatus->setText("Reference: " + QFileInfo(path).fileName());
    ui->canvasRefTarget->setImage(matToQImage(m_refMat));
}

void MainWindow::onParamChanged() {
    if (ui->chkAutoUpdate->isChecked() && !m_inputMat.empty()) {
        updateProcessing();
    }
}

void MainWindow::onRunAlgorithm() {
    if (!m_inputMat.empty()) {
        updateProcessing();
    }
}

void MainWindow::onResetDefaults() {
    m_isUpdatingUI = true;
    ui->sliderRadius->setValue(2);
    ui->spinRadius->setValue(2);

    ui->sliderEps->setValue(40);
    ui->spinEps->setValue(0.040);

    ui->sliderH->setValue(100);
    ui->spinH->setValue(10.0);

    ui->sliderGamma->setValue(20);
    ui->spinGamma->setValue(2.0);

    ui->sliderAlpha->setValue(60);
    ui->spinAlpha->setValue(0.60);

    ui->comboNorm->setCurrentIndex(0); // MinMax
    m_isUpdatingUI = false;

    updateProcessing();
}

void MainWindow::updateProcessing() {
    if (m_inputMat.empty()) return;

    ir_enhance::EnhanceParams params;
    params.guidedRadius = ui->spinRadius->value();
    params.guidedEps = static_cast<float>(ui->spinEps->value());
    params.nlmH = static_cast<float>(ui->spinH->value());
    params.gamma = static_cast<float>(ui->spinGamma->value());
    params.alpha = static_cast<float>(ui->spinAlpha->value());
    params.normMode = static_cast<ir_enhance::DetailNormMode>(ui->comboNorm->currentData().toInt());

    m_stages = ir_enhance::EnhancementPipeline::process(m_inputMat, params);

    // Update images in canvases
    QImage qOut = matToQImage(m_stages.fusedResult);
    ui->canvasEnhanced->setImage(qOut);
    ui->canvasStageOutput->setImage(qOut);
    ui->canvasRefOurs->setImage(qOut);

    // Stages: Convert floats in [0, 1] to 8-bit for display
    auto floatToQImage = [](const cv::Mat& fMat) {
        cv::Mat u8;
        cv::Mat clamped;
        cv::min(cv::max(fMat, 0.0f), 1.0f, clamped);
        clamped.convertTo(u8, CV_8U, 255.0);
        return matToQImage(u8);
    };

    ui->canvasStageBase->setImage(floatToQImage(m_stages.baseLayer));
    ui->canvasStageRawDetail->setImage(floatToQImage(m_stages.normalizedDetail));
    ui->canvasStageDenoised->setImage(floatToQImage(m_stages.denoisedDetail));
    ui->canvasStageDualGamma->setImage(floatToQImage(m_stages.enhancedDetail));

    updateMetricsDisplay();
    updateHeatmap();

    statusBar()->showMessage(QString("Enhanced in %1 ms | Size: %2x%3")
                             .arg(m_stages.elapsedMs, 0, 'f', 1)
                             .arg(m_stages.fusedResult.cols)
                             .arg(m_stages.fusedResult.rows));
}

void MainWindow::updateMetricsDisplay() {
    if (m_inputMat.empty() || m_stages.fusedResult.empty()) return;

    auto mIn = ir_enhance::EnhancementPipeline::computeMetrics(m_inputMat);
    auto mOut = ir_enhance::EnhancementPipeline::computeMetrics(m_stages.fusedResult, m_refMat);

    ui->lblTime->setText(QString("%1 ms").arg(m_stages.elapsedMs, 0, 'f', 1));
    ui->lblEntropyIn->setText(QString::number(mIn.imageEntropy, 'f', 3));
    ui->lblEntropyOut->setText(QString::number(mOut.imageEntropy, 'f', 3));
    ui->lblGEIn->setText(QString::number(mIn.gradientEntropy, 'f', 3));
    ui->lblGEOut->setText(QString::number(mOut.gradientEntropy, 'f', 3));
    ui->lblRMSIn->setText(QString::number(mIn.rmsContrast, 'f', 3));
    ui->lblRMSOut->setText(QString::number(mOut.rmsContrast, 'f', 3));

    if (!m_refMat.empty() && m_refMat.size() == m_stages.fusedResult.size()) {
        ui->lblPsnr->setText(QString("%1 dB").arg(mOut.psnr, 0, 'f', 2));
        ui->lblSsim->setText(QString::number(mOut.ssim, 'f', 3));
    } else {
        ui->lblPsnr->setText("-");
        ui->lblSsim->setText("-");
    }
}

void MainWindow::updateHeatmap() {
    if (m_refMat.empty() || m_stages.fusedResult.empty() || m_refMat.size() != m_stages.fusedResult.size()) {
        ui->canvasRefDiff->setImage(QImage());
        return;
    }

    cv::Mat diff;
    cv::absdiff(m_stages.fusedResult, m_refMat, diff);
    cv::Mat colorDiff;
    cv::applyColorMap(diff, colorDiff, cv::COLORMAP_JET);

    ui->canvasRefDiff->setImage(matToQImage(colorDiff));
}

void MainWindow::onSyncViewChanged(double scale, const QPointF& offset) {
    if (!ui->chkSyncViews->isChecked()) return;

    auto* senderCanvas = qobject_cast<ImageCanvas*>(sender());
    if (senderCanvas == ui->canvasOriginal) {
        ui->canvasEnhanced->setZoomAndPan(scale, offset);
    } else if (senderCanvas == ui->canvasEnhanced) {
        ui->canvasOriginal->setZoomAndPan(scale, offset);
    }
}

void MainWindow::onSaveOutput() {
    if (m_stages.fusedResult.empty()) return;

    QString defaultName = "enhanced_output.png";
    if (!m_currentImagePath.isEmpty()) {
        QFileInfo fi(m_currentImagePath);
        defaultName = fi.baseName() + "_enhanced.png";
    }

    QString savePath = QFileDialog::getSaveFileName(this, "Save Enhanced Image", defaultName,
                                                    "PNG Image (*.png);;JPEG Image (*.jpg);;All Files (*)");
    if (!savePath.isEmpty()) {
        cv::imwrite(savePath.toStdString(), m_stages.fusedResult);
        statusBar()->showMessage("Saved image to: " + savePath);
    }
}

void MainWindow::onExportStages() {
    if (m_stages.fusedResult.empty()) return;

    QString dirPath = QFileDialog::getExistingDirectory(this, "Select Directory to Export Stages");
    if (dirPath.isEmpty()) return;

    QFileInfo fi(m_currentImagePath);
    QString base = fi.baseName();
    if (base.isEmpty()) base = "infrared";

    auto saveFloat = [&](const cv::Mat& fMat, const QString& suffix) {
        cv::Mat u8;
        cv::Mat clamped;
        cv::min(cv::max(fMat, 0.0f), 1.0f, clamped);
        clamped.convertTo(u8, CV_8U, 255.0);
        QString outPath = QDir(dirPath).filePath(base + suffix);
        cv::imwrite(outPath.toStdString(), u8);
    };

    cv::imwrite(QDir(dirPath).filePath(base + "_1_input.png").toStdString(), m_stages.input8u);
    saveFloat(m_stages.baseLayer, "_2_base.png");
    saveFloat(m_stages.normalizedDetail, "_3_raw_detail.png");
    saveFloat(m_stages.denoisedDetail, "_4_denoised_detail.png");
    saveFloat(m_stages.enhancedDetail, "_5_dual_gamma_detail.png");
    cv::imwrite(QDir(dirPath).filePath(base + "_6_final_output.png").toStdString(), m_stages.fusedResult);

    QMessageBox::information(this, "Export Finished", "Exported 6 stages successfully to:\n" + dirPath);
}

QImage MainWindow::matToQImage(const cv::Mat& mat) {
    if (mat.empty()) return QImage();

    if (mat.type() == CV_8UC1) {
        return QImage(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_Grayscale8).copy();
    } else if (mat.type() == CV_8UC3) {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        return QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888).copy();
    } else if (mat.type() == CV_8UC4) {
        return QImage(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_ARGB32).copy();
    }
    return QImage();
}
