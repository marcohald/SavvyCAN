#include "candatagrid.h"
#include "ui_candatagrid.h"

#include <QPainter>
#include <QDebug>
#include <QMouseEvent>
#include <QRandomGenerator>

CANDataGrid::CANDataGrid(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CANDataGrid)
{
    ui->setupUi(this);

    gridMode = GridMode::CHANGED_BITS;

    memset(data, 0, 64);
    memset(refData, 0, 64);
    memset(usedData, 0, 64);
    for (int j = 0; j < 512; j++) usedSignalNum[j] = -1;
    bytesToDraw = 8; //default to the old behavior
    for (int x = 0; x < 8; x++)
        for (int y = 0; y < 64; y++)
            textStates[y][x] = GridTextState::NORMAL;

    blackBrush = QBrush(Qt::black);
    whiteBrush = QBrush(Qt::white);
    redBrush = QBrush(Qt::red);
    greenBrush = QBrush(Qt::green);
    greenHashBrush = QBrush(QColor(0, 0xB6, 0), Qt::BDiagPattern);
    blackHashBrush = QBrush(QColor(0, 0, 0), Qt::FDiagPattern);
    grayBrush = QBrush(QColor(230,230,230));
    xOffset = 0;
    yOffset = 0;
}

CANDataGrid::~CANDataGrid()
{
    delete ui;
}

GridMode CANDataGrid::getMode()
{
    return gridMode;
}

void CANDataGrid::setMode(GridMode mode)
{
    gridMode = mode;
}

void CANDataGrid::setBytesToDraw(int num)
{
    bytesToDraw = num;
    //this->update();
}

void CANDataGrid::mousePressEvent(QMouseEvent *event)
{
    QPoint clickedPoint = event->pos();
    if (event->button() == Qt::LeftButton)
    {
        //qDebug() << "Mouse Loc " << clickedPoint;
        clickedPoint -= upperLeft;
        if (clickedPoint.x() < 0 || clickedPoint.y() < 0)
        {
            //qDebug() << "Clicked outside the grid";
            return;
        }
        int x = clickedPoint.x() / gridSize.x();
        int y = clickedPoint.y() / gridSize.y();
        qDebug() << "Grid square clicked " << x << " " << y;
        int bitClicked = gridToBitPosition(x, y);
        //this control is the ultimate authority on which bit is at which grid so what we're going to do now
        //is return the actual bit so everyone else doesn't have to try to calculate it. When someone clicks
        //what the parent GUI really cares about is which bit that was.
        emit gridClicked(bitClicked);
    }
    if (event->button() == Qt::MiddleButton) //cycle through the various grid layouts for frame sizes
    {
        //qDebug() << "Middle Button";
        if (bytesToDraw < 9) bytesToDraw = 16;
        else if (bytesToDraw < 31) bytesToDraw = 32;
        else if (bytesToDraw < 63) bytesToDraw = 64;
        else if (bytesToDraw > 32) bytesToDraw = 8;
        this->update();
    }
}

void CANDataGrid::setCellTextState(int bitPos, GridTextState state)
{
    //textStates has two dimensions but they are NOT X and Y and don't necessarily correspond to X and Y in the grid
    int byte = bitPos / 8;
    int bit = bitPos & 7;
    textStates[byte][bit] = state;
    this->update();
}

GridTextState CANDataGrid::getCellTextState(int bitPos)
{
    int byte = bitPos / 8;
    int bit = bitPos & 7;
    return textStates[byte][bit];
}

void CANDataGrid::setSignalNames(int sigIdx, const QString sigName)
{
    if (sigIdx < 0) return;
    if (sigIdx >= signalNames.size())
    {
        signalNames.resize(sigIdx * 2);
    }
    signalNames[sigIdx] = sigName;
}

void CANDataGrid::clearSignalNames()
{
    signalNames.clear();
    signalNames.resize(40);
    signalColors.clear();
}

void CANDataGrid::setUsedSignalNum(int bit, int signal)
{
    if (bit < 0) return;
    if (bit > 511) return;
    usedSignalNum[bit] = signal;
}

int CANDataGrid::getUsedSignalNum(int bit)
{
    if (bit < 0) return 0;
    if (bit > 511) return 0;
    return usedSignalNum[bit];
}

void CANDataGrid::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    paintCommonBeginning();
    paintGridCells();
    paintCommonEnding();
}

/*
 * CAN-FD causes the need to support these sizes: 8, 12, 16, 20, 24, 32, 48, 64 bytes.
 * It's probably OK to ignore 12 and just go straight from 8 to 16 where each cell is subdivided in half along the X axis
 * Then 12 is just 16 minus some bits that never can get used. Then jump to 32 drawn cells from there. That would be also
 * subdividing along the Y axis. Obviously, as before, 24 is just 32 but with unusable bits. Lastly, subdivide X yet again
 * so now it's in quarters. This allows for 64 bits (48 is likewise just 64 with unusable bits)
*/
void CANDataGrid::paintCommonBeginning()
{
    int x;

    painter = new QPainter(this);
    viewport = painter->viewport();

    neededXDivisions = 8;
    neededYDivisions = 8;

    int textRestrict = qMax(neededXDivisions, neededYDivisions);

    if (bytesToDraw > 8)
    {
        neededXDivisions = 16;
    }
    if (bytesToDraw > 16)
    {
        neededXDivisions = 16;
        neededYDivisions = 16;
    }
    if (bytesToDraw > 32)
    {
        neededXDivisions = 32;
    }

    if (gridMode == GridMode::CHANGED_BITS)
    {
        bigTextSize = qMin(viewport.size().height(), viewport.size().width()) / (textRestrict * 1.5);
        smallTextSize = qMin(viewport.size().height(), viewport.size().width()) / (textRestrict * 3.5);
    }
    if (gridMode == GridMode::SIGNAL_VIEW)
    {
        bigTextSize = qMin(viewport.size().height(), viewport.size().width()) / 12;
        smallTextSize = qMin(viewport.size().height(), viewport.size().width()) / 24;
    }
    sigNameTextSize = qMin(viewport.size().height(), viewport.size().width()) / (textRestrict * 4.5);

    painter->setPen(QPen(QApplication::palette().color(QPalette::Text)));
    mainFont.setPixelSize(bigTextSize);
    painter->setFont(mainFont);
    smallFont.setPixelSize(smallTextSize);
    boldFont.setPixelSize(bigTextSize);
    boldFont.setBold(true);
    sigNameFont.setPixelSize(sigNameTextSize);

    smallMetric = new QFontMetrics(sigNameFont);

    xOffset = smallMetric->maxWidth();
    yOffset = smallMetric->height();

    xSpan = viewport.right() - viewport.left() - xOffset;
    ySpan = viewport.bottom() - viewport.top() - yOffset;

    qDebug() << "XSpan" << xSpan << " YSpan " << ySpan;

    xSector = xSpan / neededXDivisions;
    ySector = ySpan / neededYDivisions;

    qDebug() << "XSector " << xSector << " YSector " << ySector;

    nearX = viewport.left() + xOffset;
    nearY = viewport.top() + yOffset;
    farX = nearX + xSector * neededXDivisions;
    farY = nearY + ySector * neededYDivisions;

    //painter->setFont(boldFont);
    painter->setFont(sigNameFont);

    //draw grid by doing vertical and horizontal lines. This is not needed normally but helps when developing new code. Only uncomment for testing
/*
    for (int y = 0; y <= neededYDivisions; y++)
    {
        painter->drawLine(nearX, nearY + (y * ySector), farX, nearY + (y * ySector) );
    }

    for (int x = 0; x <= neededXDivisions; x++)
    {
        painter->drawLine(nearX + (x * xSector), nearY, nearX + (x * xSector), farY);
    }
*/

    for (x = 0; x < neededXDivisions; x++)
    {
        int num = (neededXDivisions - 1) - x;
        num = num & 7;
        painter->drawText(QRect(nearX + (x * xSector), viewport.top(), xSector, viewport.top() + smallMetric->height()), Qt::AlignCenter, QString::number(num));
    }

    int skip = neededXDivisions / 8;
    for (int y = 0; y < neededYDivisions; y++)
    {
        painter->drawText(QRect(viewport.left() + 2, nearY + (ySector * y), xOffset, ySector), Qt::AlignCenter, QString::number(y * skip));
    }

    painter->setPen(QPen(Qt::black));
    painter->setFont(mainFont);
}

void CANDataGrid::paintGridCells()
{
    int x, y, bit;
    unsigned char prevByte, thisByte;
    bool thisBit, prevBit;
    int usedSigNum;
    QString prevSigName;

    //if this is true then generate unique colors for each signal
    if ((signalColors.count() == 0) && (signalNames.count() > 0) && gridMode == GridMode::SIGNAL_VIEW)
    {
        qDebug() << "Generating colors";
        signalColors.resize(signalNames.count());
        for (int i = 0; i < signalNames.count(); i++)
        {
            QColor newColor;
            while (newColor.saturation() < 40)
                newColor.setRgb(QRandomGenerator::global()->bounded(160) + 60,QRandomGenerator::global()->bounded(160) + 60, QRandomGenerator::global()->bounded(160) + 60);
            qDebug() << newColor;
            signalColors[i] = newColor;
        }
    }

    //now, color the bitfield by seeing if a given bit is freshly set/unset in the new data
    //compared to the old. Bits that are not set in either are white, bits set in both are black
    //bits that used to be set but now are unset are red, bits that used to be unset but now are set
    //are green

    for (y = 0; y < neededYDivisions; y++)
    {
        for (x = 0; x < neededXDivisions; x++)
        {
            int byteIdx = (y * (neededXDivisions / 8) + (x / 8));
            thisByte = data[byteIdx];
            prevByte = refData[byteIdx];
            int bitIdx = ((neededXDivisions - 1) - x) & 7;
            bit = (byteIdx * 8) + bitIdx;
            thisBit = false;
            prevBit = false;
            if ((thisByte & (1 << bitIdx)) == (1 << bitIdx)) thisBit = true;
            if ((prevByte & (1 << bitIdx)) == (1 << bitIdx)) prevBit = true;

            if (thisBit)
            {
                if (prevBit)
                {
                    if ((signalColors.count() > 0) && (gridMode == GridMode::SIGNAL_VIEW)) painter->setBrush(blackHashBrush);
                    else painter->setBrush(blackBrush);
                }
                else
                {
                    if ((signalColors.count() > 0) && (gridMode == GridMode::SIGNAL_VIEW)) painter->setBrush(greenHashBrush);
                    else painter->setBrush(greenBrush);
                }
            }
            else
            {
                if (prevBit)
                {
                    painter->setBrush(redBrush);
                }
                else
                {                    
                    usedSigNum = -1;
                    if ((usedData[byteIdx] & (1 << bitIdx)) == (1 << bitIdx))
                    {
                        if (gridMode == GridMode::SIGNAL_VIEW) usedSigNum = getUsedSignalNum(bit);
                        if (usedSigNum == -1)
                        {
                            grayBrush = QBrush(QColor(0xB6, 0xB6, 0xB6), Qt::BDiagPattern);
                            painter->setBrush(grayBrush);
                        }
                        else painter->setBrush(QBrush(signalColors[usedSigNum]));
                    }
                    else painter->setBrush(whiteBrush);
                }
            }
            painter->drawRect(nearX + (x * xSector), nearY + (y * ySector), xSector, ySector);
            switch (textStates[byteIdx][bitIdx])
            {
            case GridTextState::NORMAL:
                if (thisBit && prevBit) painter->setPen(QPen(Qt::gray));
                else painter->setPen(QPen(Qt::black));
                painter->setFont(mainFont);
                break;
            case GridTextState::BOLD_BLUE:
                painter->setPen(QPen(Qt::blue));
                painter->setFont(boldFont);
                break;
            case GridTextState::INVERT:
                painter->setFont(mainFont);
                QColor brushColor = painter->brush().color();
                painter->setPen(QColor(255-brushColor.red(), 255-brushColor.green(), 255-brushColor.blue()));
                break;
            }
            //change style of bit number output for current signal
            //if (thisBit) painter->setFont(boldFont);
           // else painter->setFont(mainFont);
            painter->setFont(smallFont);
            if (gridMode != GridMode::SIGNAL_VIEW)
                painter->drawText(QRect(nearX + (x * xSector), nearY + (y * ySector), xSector, ySector), Qt::AlignCenter, QString::number(bit)); //center center of grid
            else
                painter->drawText(QRect(nearX + (x * xSector), nearY + (y * ySector), xSector, ySector), Qt::AlignLeft, QString::number(bit)); //upper left of grid

            painter->setFont(mainFont);
            painter->setPen(QPen(Qt::black));
        }
    }


    //now if signal names are loaded we'll go through all the bits again and try to label over top of the grid
    if (signalNames.count() > 0)
    {
        painter->setFont(sigNameFont);

        for (y = 0; y < neededYDivisions; y++)
        {
            for (x = 0; x < neededXDivisions; x++)
            {
                int byteIdx = (y * (neededXDivisions / 8) + (x / 8));
                int bitIdx = ((neededXDivisions - 1) - x) & 7;
                bit = (byteIdx * 8) + bitIdx;
                usedSigNum = -1;
                if ((usedData[byteIdx] & (1 << bitIdx)) == (1 << bitIdx))
                {
                    usedSigNum = getUsedSignalNum(bit);
                    if (prevSigName != signalNames[usedSigNum])
                    {
                        prevSigName = signalNames[usedSigNum];

                        int textWidth = smallMetric->horizontalAdvance(prevSigName);

                        if (textWidth > xSector) //signal name is too long for a single cell. Try to wrap it
                        {
                            int numAvgChars = xSector / smallMetric->averageCharWidth();
                            painter->drawText(nearX + x * xSector + 5, nearY + (y * ySector) + smallMetric->height() * 2, prevSigName.left(numAvgChars - 1));
                            QString remainder = prevSigName.mid(numAvgChars - 1, -1);
                            textWidth = smallMetric->horizontalAdvance(prevSigName);
                            if (textWidth > xSector)
                            {
                                painter->drawText(nearX + x * xSector + 12, nearY + (y * ySector) + smallMetric->height() * 3, remainder.left(numAvgChars - 1));
                            }
                            else painter->drawText(nearX + x * xSector + 12, nearY + (y * ySector) + smallMetric->height() * 3, remainder);

                        }
                        else painter->drawText(nearX + x * xSector + 5, nearY + (y * ySector) + smallMetric->height() * 2, prevSigName);
                    }
                }
            }
        }
    }
}


void CANDataGrid::paintCommonEnding()
{
    //these are used to make it easy to figure out which grid has been clicked on during mousedown events
    upperLeft.setX(nearX);
    upperLeft.setY(nearY);
    gridSize.setX(xSector);
    gridSize.setY(ySector);

    //and we don't need these anymore after we're done drawing
    delete painter;
    delete smallMetric;
}

//given a grid cell we return which bit position that is within the CAN frame.
int CANDataGrid::gridToBitPosition(int x, int y)
{
    int byteIdx = (y * (neededXDivisions / 8) + (x / 8));
    int bitIdx = ((neededXDivisions - 1) - x) & 7;
    int bit = (byteIdx * 8) + bitIdx;
    return bit;
}

//inverse of above. Given a bit position we calculate where that would be in our grid
QPoint CANDataGrid::getGridPointFromBitPosition(int bitPos)
{
    int tempBit = bitPos;
    //neededXDivisions tells us how many bits are on a single line.
    //From there we can easily determine which Y row we're in with simple division
    int y = bitPos / neededXDivisions;
    //x is more complicated because the bits go in a sort of stairstep pattern 7654321076543210
    tempBit = (bitPos - (neededXDivisions * y));
    int x = tempBit & 0xF8; //get the byte offset in the line
    x = x + (7- (tempBit & 7)); //reverse the bits in the byte

    return QPoint(x, y);
}

void CANDataGrid::saveImage(QString filename, int width, int height)
{
    Q_UNUSED(width); //currently unused but I want to use them in the future
    Q_UNUSED(height);
    //can't quite do the below commented out stuff
    //it works but doesn't scale the image into that pixmap. Need to
    //figure out how to draw the size of the pixmap
    /*
    QSize pSize;

    if (width == 0) pSize.setWidth(this->size().width());
        else pSize.setWidth(width);

    if (height == 0) pSize.setHeight(this->size().height());
        else pSize.setHeight(height);

    QPixmap pixmap(pSize);
    */
    QPixmap pixmap(this->size());

    this->render(&pixmap);
    pixmap.save(filename); //QT will automatically pick the file format given the extension
}

//these next three functions will copy the needed number of bytes from the passed buffer but you'd better have a large enough buffer or they'll get junk
//this probably won't crash the program but it would yield some really strange output. This is only really an issue for CAN-FD traffic. Make sure you
//have large enough buffers!
void CANDataGrid::setReference(unsigned char *newRef, bool bUpdate = true)
{
    int bytesToTransfer = (bytesToDraw + 7) & 0xF8; //force copying in 8 byte increments
    memcpy(refData, newRef, bytesToTransfer);
    if (bUpdate) this->update();
}

void CANDataGrid::updateData(unsigned char *newData, bool bUpdate = true)
{
    int bytesToTransfer = (bytesToDraw + 7) & 0xF8; //force copying in 8 byte increments
    memcpy(data, newData, bytesToTransfer);
    if (bUpdate) this->update();
}

void CANDataGrid::setUsed(unsigned char *newData, bool bUpdate = false)
{
    int bytesToTransfer = (bytesToDraw + 7) & 0xF8; //force copying in 8 byte increments
    memcpy(usedData, newData, bytesToTransfer);
    if (bUpdate) this->update();
}
