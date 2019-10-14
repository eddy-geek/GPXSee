#include <QPainter>
#include "map/map.h"
#include "format.h"
#include "tooltip.h"
#include "trackitem.h"


QString TrackItem::toolTip(Units units) const
{
	ToolTip tt;

	if (!_name.isEmpty())
		tt.insert(tr("Name"), _name);
	if (!_desc.isEmpty())
		tt.insert(tr("Description"), _desc);
	tt.insert(tr("Distance"), Format::distance(path().last().last().distance(),
	  units));
	if  (_time > 0)
		tt.insert(tr("Total time"), Format::timeSpan(_time));
	if  (_movingTime > 0)
		tt.insert(tr("Moving time"), Format::timeSpan(_movingTime));
	if (!_date.isNull())
		tt.insert(tr("Date"), _date.toString(Qt::SystemLocaleShortDate));
	for (int i = 0; i < _links.size(); i++) {
		const Link &link = _links.at(i);
		if (!link.URL().isEmpty()) {
			tt.insert(tr("Link"), QString("<a href=\"%0\">%1</a>").arg(
			  link.URL(), link.text().isEmpty() ? link.URL() : link.text()));
		}
	}

	return tt.toString();
}

TrackItem::TrackItem(const Track &track, Map *map, QGraphicsItem *parent)
  : PathItem(track.path(), map, parent)
{
	_name = track.name();
	_desc = track.description();
	_links = track.links();
	_date = track.date();
	_time = track.time();
	_movingTime = track.movingTime();
}
