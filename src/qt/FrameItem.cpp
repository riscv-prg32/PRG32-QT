#include "FrameItem.h"
#include <QPainter>
void FrameItem::setFrame(const std::vector<uint16_t>&p){QImage im(320,200,QImage::Format_RGB888);for(int y=0;y<200;y++){auto*row=im.scanLine(y);for(int x=0;x<320;x++){uint16_t c=p[y*320+x];row[x*3+0]=uint8_t(((c>>11)&31)*255/31);row[x*3+1]=uint8_t(((c>>5)&63)*255/63);row[x*3+2]=uint8_t((c&31)*255/31);}}image_=std::move(im);update();}
void FrameItem::paint(QPainter*p){p->setRenderHint(QPainter::SmoothPixmapTransform,false);p->fillRect(boundingRect(),Qt::black);if(!image_.isNull())p->drawImage(boundingRect(),image_);}
