#ifndef COORDINATESITEM_H
#define COORDINATESITEM_H

#include <QGraphicsItem>
#include <QFont>
#include "common/coordinates.h"
#include "format.h"

class CoordinatesItem : public QGraphicsItem
{
public:
	CoordinatesItem(QGraphicsItem *parent = 0);

	QRectF boundingRect() const {return _boundingRect;}
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
	  QWidget *widget);

	CoordinatesFormat format() const {return _format;}

	void setCoordinates(const Coordinates &c, qreal elevation = NAN);
	void setExtraCoord(const QPointF &xy, const QRectF &bounds, int zoom, qreal res);
	void setFormat(CoordinatesFormat format);
	void setUnits(Units units);
	void setDigitalZoom(qreal zoom);
	void setColor(const QColor &color);
	void setBackgroundColor(const QColor &color);
	void drawBackground(bool draw);

private:
	void updateBoundingRect();
	QString text() const;

	Coordinates _c;
	qreal _ele;
	QPointF _xy;
	QRectF _bounds;
	int _zoom;
	qreal _res;

	CoordinatesFormat _format;
	Units _units;
	QRectF _boundingRect;
	QFont _font;
	qreal _digitalZoom;
	QColor _color, _bgColor;
	bool _drawBackground;
};

#endif // COORDINATESITEM_H
