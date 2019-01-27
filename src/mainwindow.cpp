#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QShortcut>
#include <QFileDialog>
#include <QFileInfo>
#include <QSettings>
#include <QDragEnterEvent>
#include <QImageReader>
#include <QMimeData>

const QString DEFAULT_DIR_KEY = "DefaultDir";


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    sheet(2)
{
    ui->setupUi(this);
    connect(new QShortcut(Qt::Key_Space,  this), &QShortcut::activated, this, &MainWindow::on_btSwitch_clicked);
    connect(new QShortcut(Qt::Key_Plus,   this), &QShortcut::activated, this, &MainWindow::zoomIn);
    connect(new QShortcut(Qt::Key_Minus,  this), &QShortcut::activated, this, &MainWindow::zoomOut);
    connect(new QShortcut(Qt::Key_0,      this), &QShortcut::activated, this, &MainWindow::zoomReset);
    connect(new QShortcut(Qt::Key_Insert, this), &QShortcut::activated, this, &MainWindow::zoomReset);

    this->paletteDefault = this->paletteMagenta = this->paletteGreen = ui->btOpen1->palette();
    this->paletteMagenta.setColor(QPalette::Button, Qt::magenta);
    this->paletteGreen.setColor(QPalette::Button, Qt::green);

    this->setWindowState(Qt::WindowMaximized);

    const QByteArrayList supportedImageFormats = QImageReader::supportedImageFormats();
    for (const QByteArray& format : supportedImageFormats)
    {
        this->imageFormats.append(format);
    }
    this->imageFormats.sort();
    this->imageFormatsFilter = QString("Изображения (*.%1)").arg(this->imageFormats.join(" *."));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        const QList<QUrl> urls = event->mimeData()->urls();
        for (const QUrl &url : urls)
        {
            if (!this->urlToPath(url).isEmpty())
            {
                event->acceptProposedAction();
                return;
            }
        }
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        for (int i = 0; i < event->mimeData()->urls().size() && i < this->sheet.size(); ++i)
        {
            QString path = this->urlToPath(event->mimeData()->urls().at(i));
            if (!path.isEmpty())
            {
                loadImage(i, path);
            }
        }
        event->acceptProposedAction();
    }
}

void MainWindow::on_btOpen1_clicked()
{
    loadImage(0);
}

void MainWindow::on_btOpen2_clicked()
{
    loadImage(1);
}

void MainWindow::on_btSwitch_clicked()
{
    if (ui->btSwitch->isEnabled()) switchImage();
}

void MainWindow::on_slZoom_valueChanged(const int value)
{
    const qreal factor = static_cast<qreal>(value) / 100.0;

    ui->graphicsView->resetTransform();
    ui->graphicsView->scale(factor, factor);

    ui->edPercent->setText(QString("%1%").arg(value));
}

void MainWindow::on_slZoom_customContextMenuRequested(const QPoint &pos)
{
    Q_UNUSED(pos);
    ui->slZoom->setValue(100);
}

void MainWindow::on_graphicsView_customContextMenuRequested(const QPoint &pos)
{
    Q_UNUSED(pos);
    on_btSwitch_clicked();
}

void MainWindow::zoomIn()
{
    if (ui->slZoom->isEnabled()) ui->slZoom->triggerAction(QAbstractSlider::SliderPageStepAdd);
}

void MainWindow::zoomOut()
{
    if (ui->slZoom->isEnabled()) ui->slZoom->triggerAction(QAbstractSlider::SliderPageStepSub);
}

void MainWindow::zoomReset()
{
    if (ui->slZoom->isEnabled()) ui->slZoom->setValue(100);
}


void MainWindow::loadImage(const int pos, QString fileName)
{
    if (fileName.isEmpty())
    {
        QSettings settings;
        fileName = QFileDialog::getOpenFileName(this,
                                                QString("Выберите изображение %1").arg(pos + 1),
                                                settings.value(DEFAULT_DIR_KEY).toString(),
                                                this->imageFormatsFilter);

        if (fileName.isEmpty()) return;
        settings.setValue(DEFAULT_DIR_KEY, QFileInfo(fileName).absolutePath());
    }

    this->sheet[pos].clear();
    this->sheet[pos].scene = new QGraphicsScene();
    this->sheet[pos].pixmap = new QPixmap(fileName);

    const int other = (pos + 1) % this->sheet.size();

    // Масштабируем
    if ( this->sheet[other].pixmap && this->sheet[other].scene &&
         ( this->sheet[other].pixmap->width() > this->sheet[pos].pixmap->width() ||
           this->sheet[other].pixmap->height() > this->sheet[pos].pixmap->height() ) )
    {
        this->sheet[pos].scene->addPixmap( this->sheet[pos].pixmap->scaled(
            this->sheet[other].pixmap->size(),
            Qt::IgnoreAspectRatio,
            Qt::SmoothTransformation) );
        this->sheet[pos].scaled = true;

        if (this->sheet[other].scaled)
        {
            delete this->sheet[other].scene;
            this->sheet[other].scene = new QGraphicsScene();
            this->sheet[other].scene->addPixmap(*(this->sheet[other].pixmap));
            this->sheet[other].scaled = false;
        }
    }
    else
    {
        this->sheet[pos].scene->addPixmap(*(this->sheet[pos].pixmap));
        this->sheet[pos].scaled = false;

        if (this->sheet[other].pixmap && sheet[other].scene)
        {
            if (this->sheet[other].pixmap->width() < this->sheet[pos].pixmap->width() ||
                this->sheet[other].pixmap->height() < this->sheet[pos].pixmap->height())
            {
                delete this->sheet[other].scene;
                this->sheet[other].scene = new QGraphicsScene();
                this->sheet[other].scene->addPixmap( this->sheet[other].pixmap->scaled(
                    this->sheet[pos].pixmap->size(),
                    Qt::IgnoreAspectRatio,
                    Qt::SmoothTransformation) );
                this->sheet[other].scaled = true;
            }
            else if (this->sheet[other].scaled)
            {
                delete this->sheet[other].scene;
                this->sheet[other].scene = new QGraphicsScene();
                this->sheet[other].scene->addPixmap(*(this->sheet[other].pixmap));
                this->sheet[other].scaled = false;
            }
        }
    }

    this->sheet[pos].name = QFileInfo(fileName).fileName();
    if (pos) {
        ui->btOpen2->setText(this->sheet[pos].name);
    }
    else {
        ui->btOpen1->setText(this->sheet[pos].name);
    }

    // Центрирование изображения
    ui->slZoom->setValue(100);
    switchImage(pos);
    ui->graphicsView->centerOn(QPointF(this->sheet[pos].scene->width() / 2.0,
                                       this->sheet[pos].scene->height() / 2.0));

    // Выбор контрастного цвета
    /*QColor color(this->sheet[pos].pixmap->toImage().pixel(5, 5));
    if ((0.3 * color.redF() + 0.59 * color.greenF() + 0.11 * color.blueF()) > 0.25)
    {
        this->sheet[pos].namePalette.setColor(ui->lbName->foregroundRole(), Qt::black);
    }
    else
    {
        this->sheet[pos].namePalette.setColor(ui->lbName->foregroundRole(), Qt::white);
    }*/

    ui->graphicsView->setEnabled(true);
    ui->btSwitch->setEnabled(true);
    ui->slZoom->setEnabled(true);
}

void MainWindow::switchImage(const int pos)
{
    static int lastPos = this->sheet.size() - 1;

    if (pos >= this->sheet.size())
    {
        lastPos = (lastPos + 1) % this->sheet.size();
    }
    else
    {
        lastPos = pos;
    }

    if (this->sheet[lastPos].scene)
    {
        QPointF center = ui->graphicsView->mapToScene(ui->graphicsView->viewport()->rect()).boundingRect().center();
        ui->graphicsView->setScene(this->sheet[lastPos].scene);
        ui->graphicsView->centerOn(center);

        if (lastPos) {
            ui->btOpen1->setPalette(this->paletteDefault);
            ui->btOpen2->setPalette(this->paletteGreen);
        }
        else {
            ui->btOpen2->setPalette(this->paletteDefault);
            ui->btOpen1->setPalette(this->paletteMagenta);
        }
    }
}

QString MainWindow::urlToPath(const QUrl &url)
{
    if (url.isLocalFile()) {
        const QString path = url.toLocalFile();
        if (this->imageFormats.contains(QFileInfo(path).suffix(), Qt::CaseInsensitive)) {
            return path;
        }
    }
    return QString::null;
}
