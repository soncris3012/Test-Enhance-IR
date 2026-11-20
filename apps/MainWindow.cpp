#include "MainWindow.hpp"
#include "ui_MainWindow.h"
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QStatusBar>
#include <QSignalBlocker>
#include <QSettings>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <array>
#include <cmath>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), ui(std::make_unique<Ui::MainWindow>()) {
    ui->setupUi(this);

    // Clean, minimalist neutral styling (non-distracting)
    setStyleSheet(R"(
        * { color: #dcdcdc; font-size: 12px; }
        QMainWindow, QWidget#centralWidget { background-color: #2b2b2b; }
        QScrollArea#sidebarScroll { background-color: #262626; border: none; border-right: 1px solid #404040; }
        QScrollArea#sidebarScroll > QWidget > QWidget { background-color: #262626; }
        QWidget#sidebar { background-color: #262626; }
        QScrollBar:vertical {
            background: #262626;
            width: 8px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #4a4a4a;
            min-height: 20px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background: #6a6a6a;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QGroupBox {
            font-weight: 600;
            border: 1px solid #505050;
            border-radius: 4px;
            margin-top: 10px;
            padding-top: 12px;
            background-color: #303030;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 8px;
            padding: 0 4px;
            color: #eeeeee;
        }
        QPushButton {
            background-color: #3c3c3c;
            border: 1px solid #606060;
            border-radius: 3px;
            padding: 4px 8px;
            font-weight: 600;
        }
        QPushButton:hover { background-color: #484848; border-color: #808080; }
        QPushButton:pressed { background-color: #2a2a2a; }
        QPushButton#btnRun {
            background-color: #3b4c62;
            border: 1px solid #6c809a;
            color: #ffffff;
            font-weight: bold;
        }
        QPushButton#btnRun:hover { background-color: #485e79; }
        QComboBox, QSpinBox, QDoubleSpinBox {
            background-color: #222222;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 3px 6px;
            color: #eeeeee;
        }
        QComboBox:disabled {
            background-color: #2a2a2a;
            color: #777777;
            border-color: #424242;
        }
        QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus { border-color: #888888; }
        QSlider::groove:horizontal {
            height: 4px;
            background: #444444;
            border-radius: 2px;
        }
        QSlider::sub-page:horizontal {
            background: #5b6d85;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: #d0d0d0;
            border: 1px solid #707070;
            width: 12px;
            margin-top: -4px;
            margin-bottom: -4px;
            border-radius: 6px;
        }
        QSlider::handle:horizontal:hover { background: #ffffff; }
        QTabWidget::pane { border: 1px solid #454545; background-color: #242424; }
        QTabBar::tab {
            background: #303030;
            color: #aaaaaa;
            padding: 7px 14px;
            border: 1px solid #454545;
            border-bottom: none;
            border-top-left-radius: 3px;
            border-top-right-radius: 3px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background: #242424;
            color: #ffffff;
            font-weight: 600;
        }
        QTabBar::tab:hover:!selected { background: #383838; }
        QLabel { color: #cccccc; }
        QCheckBox { color: #d0d0d0; spacing: 6px; }
        QCheckBox::indicator {
            width: 14px;
            height: 14px;
            border: 1px solid #606060;
            border-radius: 2px;
            background: #252525;
        }
        QCheckBox::indicator:checked {
            background: #4a678f;
            border-color: #7294c2;
        }
        QStatusBar { background-color: #1f1f1f; color: #888888; border-top: 1px solid #333333; }
    )");

    // Titles for image canvases
    ui->canvasOriginal->setTitle("Original Infrared Input");
    ui->canvasEnhanced->setTitle("Enhanced Output");

    ui->canvasStageInput->setTitle("1. Original Image (I)");
    ui->canvasStageBase->setTitle("2. Base Layer (Guided Filter I_base)");
    ui->canvasStageRawDetail->setTitle("3. Raw Detail Layer (Normalized)");
    ui->canvasStageDenoised->setTitle("4. Denoised Detail (16-bit NLM)");
    ui->canvasStageDualGamma->setTitle("5. Dual-Gamma Detail");
    ui->canvasStageOutput->setTitle("6. Final Output (I_out)");

    // Populate Mode selector
    ui->comboMode->addItem("Symmetric Dual-Gamma (Recommended)",
                           static_cast<int>(ir_enhance::PipelineMode::SymmetricDualGamma));
    ui->comboMode->addItem("Paper Magnitude (Fig 2c / 3c)",
                           static_cast<int>(ir_enhance::PipelineMode::PaperMagnitudeFusion));
    ui->comboMode->addItem("Paper Literal Eq. 8 (Original)",
                           static_cast<int>(ir_enhance::PipelineMode::PaperLiteralEq8));

    // Populate Detail Normalization selector
    ui->comboNorm->addItem("Min-Max (Paper Eq)", static_cast<int>(ir_enhance::DetailNormMode::MinMax));
    ui->comboNorm->addItem("Shift (+0.5)", static_cast<int>(ir_enhance::DetailNormMode::Shift05));
    ui->comboNorm->addItem("Symmetric Centered", static_cast<int>(ir_enhance::DetailNormMode::Symmetric));
    ui->comboNorm->setToolTip("Detail normalization used in Paper Literal Eq. 8 mode; symmetric zero-preserving scaling is used otherwise.");

    m_sampleDir = locateSampleDirectory();

    setupConnections();
    onModeChanged();
    initializeDatasets();
}

MainWindow::~MainWindow() = default;

QString MainWindow::locateSampleDirectory() {
    const QStringList roots = {
        QCoreApplication::applicationDirPath(),
        QDir::currentPath()
    };
    for (const QString& start : roots) {
        QDir dir(start);
        for (int depth = 0; depth <= 4; ++depth) {
            if (dir.exists("sample_images")) {
                return dir.filePath("sample_images");
            }
            if (!dir.cdUp()) {
                break;
            }
        }
    }
    return QDir::currentPath();
}

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

    // File & Dataset navigation
    connect(ui->btnOpen, &QPushButton::clicked, this, &MainWindow::onOpenImage);
    connect(ui->btnOpenFolder, &QPushButton::clicked, this, &MainWindow::onOpenFolder);
    connect(ui->btnLoadRef, &QPushButton::clicked, this, &MainWindow::onLoadReference);
    connect(ui->btnSetRoot, &QPushButton::clicked, this, &MainWindow::onSetDatasetRoot);
    connect(ui->comboDataset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onDatasetSelected);
    connect(ui->comboFiles, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onFileSelected);
    connect(ui->btnPrev, &QPushButton::clicked, this, &MainWindow::onPrevImage);
    connect(ui->btnNext, &QPushButton::clicked, this, &MainWindow::onNextImage);

    // Other inputs
    connect(ui->comboMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onModeChanged);
    connect(ui->comboNorm, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onParamChanged);
    connect(ui->chkAdaptiveNlm, &QCheckBox::toggled, this, &MainWindow::onParamChanged);
    connect(ui->btnRun, &QPushButton::clicked, this, &MainWindow::onRunAlgorithm);
    connect(ui->btnReset, &QPushButton::clicked, this, &MainWindow::onResetDefaults);
    connect(ui->btnSave, &QPushButton::clicked, this, &MainWindow::onSaveOutput);
    connect(ui->btnExportStages, &QPushButton::clicked, this, &MainWindow::onExportStages);

    // Sync zoom/pan & double-click reset across canvases in each tab
    const std::array<ImageCanvas*, 8> allCanvases = {
        ui->canvasOriginal, ui->canvasEnhanced,
        ui->canvasStageInput, ui->canvasStageBase, ui->canvasStageRawDetail,
        ui->canvasStageDenoised, ui->canvasStageDualGamma, ui->canvasStageOutput
    };
    for (ImageCanvas* canvas : allCanvases) {
        connect(canvas, &ImageCanvas::viewChanged, this, &MainWindow::onSyncViewChanged);
        connect(canvas, &ImageCanvas::viewReset, this, &MainWindow::onSyncViewReset);
    }
}

void MainWindow::initializeDatasets() {
    QSettings settings("DeepMindLab", "InfraredDetailEnhancement");
    const QString savedRoot = settings.value("datasetRoot").toString();

    std::string detected = ir_enhance::DatasetManager::detectDatasetRoot(savedRoot.toStdString());
    m_datasetRoot = QString::fromStdString(detected);

    refreshDatasets(m_datasetRoot);
}

void MainWindow::refreshDatasets(const QString& rootPath) {
    ui->comboDataset->blockSignals(true);
    ui->comboDataset->clear();

    m_datasets = ir_enhance::DatasetManager::discoverDatasets(rootPath.toStdString(), m_sampleDir.toStdString());

    if (m_datasets.empty()) {
        ui->comboDataset->addItem("-- No Datasets Detected --", "");
    } else {
        for (const auto& entry : m_datasets) {
            ui->comboDataset->addItem(QString::fromStdString(entry.displayName),
                                      QString::fromStdString(entry.folderPath));
        }
    }

    ui->comboDataset->addItem("[ Browse Custom Folder... ]", "__BROWSE_FOLDER__");

    if (rootPath.isEmpty()) {
        ui->lblDatasetStatus->setText("Root: Built-in only (Click '...' to set root)");
    } else {
        QString compact = rootPath;
        const QString home = QDir::homePath();
        if (compact.startsWith(home)) {
            compact.replace(0, home.length(), "~");
        }
        ui->lblDatasetStatus->setText("Root: " + compact);
    }

    ui->comboDataset->blockSignals(false);

    if (!m_datasets.empty()) {
        ui->comboDataset->setCurrentIndex(0);
        loadDataset(0);
    }
}

void MainWindow::loadDataset(int datasetIdx) {
    if (datasetIdx < 0 || datasetIdx >= static_cast<int>(m_datasets.size())) return;

    const auto& entry = m_datasets[datasetIdx];
    m_currentFiles = ir_enhance::DatasetManager::scanImageFiles(entry.folderPath);

    ui->comboFiles->blockSignals(true);
    ui->comboFiles->clear();

    const int total = static_cast<int>(m_currentFiles.size());
    if (total == 0) {
        ui->comboFiles->addItem("No images found", "");
        ui->btnPrev->setEnabled(false);
        ui->btnNext->setEnabled(false);
    } else {
        for (int i = 0; i < total; ++i) {
            const QString fname = QFileInfo(QString::fromStdString(m_currentFiles[i])).fileName();
            ui->comboFiles->addItem(QString("%1 [%2/%3]").arg(fname).arg(i + 1).arg(total),
                                    QString::fromStdString(m_currentFiles[i]));
        }
        ui->btnPrev->setEnabled(false);
        ui->btnNext->setEnabled(total > 1);
    }

    ui->comboFiles->blockSignals(false);

    if (total > 0) {
        onFileSelected(0);
    }
}

void MainWindow::onDatasetSelected(int index) {
    if (index < 0) return;
    const QString data = ui->comboDataset->itemData(index).toString();
    if (data == "__BROWSE_FOLDER__") {
        onOpenFolder();
        return;
    }
    loadDataset(index);
}

void MainWindow::onFileSelected(int index) {
    if (index < 0 || index >= static_cast<int>(m_currentFiles.size())) return;
    m_currentFileIdx = index;

    const QString path = QString::fromStdString(m_currentFiles[index]);
    const QFileInfo fi(path);

    // If Paper sample, try auto-loading matched reference
    QString refPath;
    if (fi.baseName().endsWith("_original")) {
        QString refName = fi.baseName();
        refName.replace("_original", "_paper_ours.png");
        const QString candidate = fi.dir().filePath(refName);
        if (QFile::exists(candidate)) {
            refPath = candidate;
        }
    }
    loadReference(refPath);
    loadImage(path);

    ui->btnPrev->setEnabled(index > 0);
    ui->btnNext->setEnabled(index + 1 < static_cast<int>(m_currentFiles.size()));
}

void MainWindow::onPrevImage() {
    if (m_currentFileIdx > 0) {
        ui->comboFiles->setCurrentIndex(m_currentFileIdx - 1);
    }
}

void MainWindow::onNextImage() {
    if (m_currentFileIdx + 1 < static_cast<int>(m_currentFiles.size())) {
        ui->comboFiles->setCurrentIndex(m_currentFileIdx + 1);
    }
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Left) {
        onPrevImage();
        event->accept();
        return;
    } else if (event->key() == Qt::Key_Right) {
        onNextImage();
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::onOpenFolder() {
    const QString dir = QFileDialog::getExistingDirectory(this, "Select Custom Infrared Image Directory",
                                                          m_datasetRoot.isEmpty() ? m_sampleDir : m_datasetRoot);
    if (dir.isEmpty()) return;

    auto files = ir_enhance::DatasetManager::scanImageFiles(dir.toStdString());
    if (files.empty()) {
        QMessageBox::warning(this, "No Images", "No supported infrared images found in:\n" + dir);
        return;
    }

    ir_enhance::DatasetEntry custom;
    custom.id = "custom_" + QFileInfo(dir).fileName().toStdString();
    custom.displayName = "Custom: " + QFileInfo(dir).fileName().toStdString() +
                         " (" + std::to_string(files.size()) + " imgs)";
    custom.folderPath = dir.toStdString();
    custom.imageCount = static_cast<int>(files.size());

    m_datasets.push_back(custom);
    const int newIdx = static_cast<int>(m_datasets.size()) - 1;

    ui->comboDataset->blockSignals(true);
    // Insert right before "__BROWSE_FOLDER__"
    ui->comboDataset->insertItem(newIdx, QString::fromStdString(custom.displayName), QString::fromStdString(custom.folderPath));
    ui->comboDataset->setCurrentIndex(newIdx);
    ui->comboDataset->blockSignals(false);

    loadDataset(newIdx);
}

void MainWindow::onSetDatasetRoot() {
    const QString dir = QFileDialog::getExistingDirectory(this, "Select Dataset Root Directory (e.g. QART_Datasets/raw)",
                                                          m_datasetRoot.isEmpty() ? QDir::homePath() : m_datasetRoot);
    if (dir.isEmpty()) return;

    QSettings settings("DeepMindLab", "InfraredDetailEnhancement");
    settings.setValue("datasetRoot", dir);

    std::string detected = ir_enhance::DatasetManager::detectDatasetRoot(dir.toStdString());
    m_datasetRoot = QString::fromStdString(detected);
    refreshDatasets(m_datasetRoot);
}

void MainWindow::onOpenImage() {
    const QString path = QFileDialog::getOpenFileName(this, "Open Infrared Image", m_sampleDir,
                                                      "Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;All Files (*)");
    if (!path.isEmpty()) {
        loadReference("");
        loadImage(path);
    }
}

void MainWindow::onLoadReference() {
    const QString path = QFileDialog::getOpenFileName(this, "Open Reference/Ground Truth Image", m_sampleDir,
                                                      "Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;All Files (*)");
    if (!path.isEmpty()) {
        loadReference(path);
        updateMetricsDisplay();
    }
}

void MainWindow::loadImage(const QString& path) {
    // IMREAD_ANYDEPTH | IMREAD_ANYCOLOR preserves 16-bit thermal TIFF dynamic range
    cv::Mat img = cv::imread(path.toStdString(), cv::IMREAD_ANYDEPTH | cv::IMREAD_ANYCOLOR);
    if (img.empty()) {
        QMessageBox::warning(this, "Error", "Failed to load image: " + path);
        return;
    }
    m_inputMat = img;
    m_currentImagePath = path;

    updateProcessing();
    statusBar()->showMessage(QString("Loaded: %1 (%2x%3, depth=%4) | Enhanced in %5 ms")
                             .arg(QFileInfo(path).fileName())
                             .arg(img.cols)
                             .arg(img.rows)
                             .arg(img.depth() == CV_16U ? "16-bit" : (img.depth() == CV_8U ? "8-bit" : "float"))
                             .arg(m_stages.elapsedMs, 0, 'f', 1));
}

void MainWindow::loadReference(const QString& path) {
    if (path.isEmpty()) {
        m_refMat = cv::Mat();
        ui->lblRefStatus->setText("Reference: None (Real Dataset)");
        return;
    }

    cv::Mat ref = cv::imread(path.toStdString(), cv::IMREAD_ANYDEPTH | cv::IMREAD_ANYCOLOR);
    if (ref.empty()) {
        m_refMat = cv::Mat();
        ui->lblRefStatus->setText("Reference: Failed to load");
        return;
    }
    if (ref.channels() == 3) {
        cv::cvtColor(ref, ref, cv::COLOR_BGR2GRAY);
    } else if (ref.channels() == 4) {
        cv::cvtColor(ref, ref, cv::COLOR_BGRA2GRAY);
    }
    if (ref.depth() != CV_8U) {
        cv::normalize(ref, ref, 0, 255, cv::NORM_MINMAX, CV_8U);
    }
    m_refMat = ref;
    ui->lblRefStatus->setText("Reference: " + QFileInfo(path).fileName());
}

void MainWindow::onModeChanged() {
    const auto mode = static_cast<ir_enhance::PipelineMode>(ui->comboMode->currentData().toInt());
    const bool useLiteralNorm = (mode == ir_enhance::PipelineMode::PaperLiteralEq8);
    ui->lblNorm->setEnabled(useLiteralNorm);
    ui->comboNorm->setEnabled(useLiteralNorm);
    onParamChanged();
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

    ui->comboMode->setCurrentIndex(0);
    ui->comboNorm->setCurrentIndex(0);
    ui->chkAdaptiveNlm->setChecked(true);
    m_isUpdatingUI = false;

    onModeChanged();
    updateProcessing();
}

void MainWindow::updateProcessing() {
    if (m_inputMat.empty()) return;

    ir_enhance::EnhanceParams params;
    params.guidedRadius = ui->spinRadius->value();
    params.guidedEps = static_cast<float>(ui->spinEps->value());
    params.nlmH = static_cast<float>(ui->spinH->value());
    params.adaptiveNlm = ui->chkAdaptiveNlm->isChecked();
    params.gamma = static_cast<float>(ui->spinGamma->value());
    params.alpha = static_cast<float>(ui->spinAlpha->value());
    params.mode = static_cast<ir_enhance::PipelineMode>(ui->comboMode->currentData().toInt());
    params.normMode = static_cast<ir_enhance::DetailNormMode>(ui->comboNorm->currentData().toInt());

    m_stages = ir_enhance::EnhancementPipeline::process(m_inputMat, params);

    const QImage qIn = matToQImage(m_stages.input8u);
    const QImage qOut = matToQImage(m_stages.fusedResult);

    ui->canvasOriginal->setImage(qIn);
    ui->canvasStageInput->setImage(qIn);
    ui->canvasEnhanced->setImage(qOut);
    ui->canvasStageOutput->setImage(qOut);

    auto floatToQImage = [](const cv::Mat& fMat) {
        cv::Mat clamped, u8;
        cv::min(cv::max(fMat, 0.0f), 1.0f, clamped);
        clamped.convertTo(u8, CV_8U, 255.0);
        return matToQImage(u8);
    };

    ui->canvasStageBase->setImage(floatToQImage(m_stages.baseLayer));
    ui->canvasStageRawDetail->setImage(floatToQImage(m_stages.normalizedDetail));
    ui->canvasStageDenoised->setImage(floatToQImage(m_stages.denoisedDetail));
    ui->canvasStageDualGamma->setImage(floatToQImage(m_stages.enhancedDetail));

    updateMetricsDisplay();

    statusBar()->showMessage(QString("Enhanced in %1 ms | Size: %2x%3")
                             .arg(m_stages.elapsedMs, 0, 'f', 1)
                             .arg(m_stages.fusedResult.cols)
                             .arg(m_stages.fusedResult.rows));
}

void MainWindow::updateMetricsDisplay() {
    if (m_stages.input8u.empty() || m_stages.fusedResult.empty()) return;

    const bool hasRef = !m_refMat.empty() && m_refMat.size() == m_stages.fusedResult.size();
    const cv::Mat& cmpTarget = hasRef ? m_refMat : m_stages.input8u;
    const QString targetSuffix = hasRef ? "(vs Ref):" : "(vs Input):";

    const auto mIn = ir_enhance::EnhancementPipeline::computeMetrics(m_stages.input8u);
    const auto mOut = ir_enhance::EnhancementPipeline::computeMetrics(m_stages.fusedResult, cmpTarget);

    ui->lblTime->setText(QString("%1 ms").arg(m_stages.elapsedMs, 0, 'f', 1));
    ui->lblEntropyIn->setText(QString::number(mIn.imageEntropy, 'f', 3));
    ui->lblEntropyOut->setText(QString::number(mOut.imageEntropy, 'f', 3));
    ui->lblGEIn->setText(QString::number(mIn.gradientEntropy, 'f', 3));
    ui->lblGEOut->setText(QString::number(mOut.gradientEntropy, 'f', 3));
    ui->lblRMSIn->setText(QString::number(mIn.rmsContrast, 'f', 3));
    ui->lblRMSOut->setText(QString::number(mOut.rmsContrast, 'f', 3));

    ui->lblTitleRmse->setText("RMSE " + targetSuffix);
    ui->lblTitlePsnr->setText("PSNR " + targetSuffix);
    ui->lblTitleSsim->setText("SSIM " + targetSuffix);

    ui->lblRmse->setText(QString::number(mOut.rmse, 'f', 4));
    ui->lblPsnr->setText(QString("%1 dB").arg(mOut.psnr, 0, 'f', 2));
    ui->lblSsim->setText(QString::number(mOut.ssim, 'f', 3));
}

void MainWindow::onSyncViewChanged(double scale, const QPointF& offset) {
    if (!ui->chkSyncViews->isChecked()) return;

    auto* senderCanvas = qobject_cast<ImageCanvas*>(sender());
    if (!senderCanvas) return;

    const std::array<ImageCanvas*, 2> tab1 = {ui->canvasOriginal, ui->canvasEnhanced};
    const std::array<ImageCanvas*, 6> tab2 = {
        ui->canvasStageInput, ui->canvasStageBase, ui->canvasStageRawDetail,
        ui->canvasStageDenoised, ui->canvasStageDualGamma, ui->canvasStageOutput
    };

    auto syncGroup = [&](auto& group) {
        if (std::find(group.begin(), group.end(), senderCanvas) != group.end()) {
            for (ImageCanvas* c : group) {
                if (c != senderCanvas) {
                    c->setZoomAndPan(scale, offset);
                }
            }
        }
    };
    syncGroup(tab1);
    syncGroup(tab2);
}

void MainWindow::onSyncViewReset() {
    if (!ui->chkSyncViews->isChecked()) return;

    auto* senderCanvas = qobject_cast<ImageCanvas*>(sender());
    if (!senderCanvas) return;

    const std::array<ImageCanvas*, 2> tab1 = {ui->canvasOriginal, ui->canvasEnhanced};
    const std::array<ImageCanvas*, 6> tab2 = {
        ui->canvasStageInput, ui->canvasStageBase, ui->canvasStageRawDetail,
        ui->canvasStageDenoised, ui->canvasStageDualGamma, ui->canvasStageOutput
    };

    auto resetGroup = [&](auto& group) {
        if (std::find(group.begin(), group.end(), senderCanvas) != group.end()) {
            for (ImageCanvas* c : group) {
                if (c != senderCanvas) {
                    c->applyAutoFit();
                }
            }
        }
    };
    resetGroup(tab1);
    resetGroup(tab2);
}

void MainWindow::onSaveOutput() {
    if (m_stages.fusedResult.empty()) return;

    QString defaultName = "enhanced_output.png";
    if (!m_currentImagePath.isEmpty()) {
        const QFileInfo fi(m_currentImagePath);
        defaultName = fi.baseName() + "_enhanced.png";
    }

    QString savePath = QFileDialog::getSaveFileName(this, "Save Enhanced Image", defaultName,
                                                    "PNG Image (*.png);;JPEG Image (*.jpg);;All Files (*)");
    if (savePath.isEmpty()) return;

    if (QFileInfo(savePath).suffix().isEmpty()) {
        savePath += ".png";
    }

    try {
        if (!cv::imwrite(savePath.toStdString(), m_stages.fusedResult)) {
            QMessageBox::warning(this, "Save Failed", "Could not write output image to:\n" + savePath);
            return;
        }
        statusBar()->showMessage("Saved image to: " + savePath);
    } catch (const cv::Exception& ex) {
        QMessageBox::warning(this, "Save Failed", QString("OpenCV error while saving:\n%1").arg(ex.what()));
    }
}

void MainWindow::onExportStages() {
    if (m_stages.fusedResult.empty()) return;

    const QString dirPath = QFileDialog::getExistingDirectory(this, "Select Directory to Export Stages");
    if (dirPath.isEmpty()) return;

    const QFileInfo fi(m_currentImagePath);
    QString base = fi.baseName();
    if (base.isEmpty()) base = "infrared";

    bool allOk = true;
    auto writeMat = [&](const cv::Mat& mat, const QString& suffix) {
        const QString outPath = QDir(dirPath).filePath(base + suffix);
        try {
            if (!cv::imwrite(outPath.toStdString(), mat)) {
                allOk = false;
            }
        } catch (const cv::Exception&) {
            allOk = false;
        }
    };

    auto saveFloat = [&](const cv::Mat& fMat, const QString& suffix) {
        cv::Mat clamped, u8;
        cv::min(cv::max(fMat, 0.0f), 1.0f, clamped);
        clamped.convertTo(u8, CV_8U, 255.0);
        writeMat(u8, suffix);
    };

    writeMat(m_stages.input8u, "_1_input.png");
    saveFloat(m_stages.baseLayer, "_2_base.png");
    saveFloat(m_stages.normalizedDetail, "_3_raw_detail.png");
    saveFloat(m_stages.denoisedDetail, "_4_denoised_detail.png");
    saveFloat(m_stages.enhancedDetail, "_5_dual_gamma_detail.png");
    writeMat(m_stages.fusedResult, "_6_final_output.png");

    if (allOk) {
        QMessageBox::information(this, "Export Finished", "Exported 6 stages successfully to:\n" + dirPath);
    } else {
        QMessageBox::warning(this, "Export Incomplete", "Some files could not be written to:\n" + dirPath);
    }
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
