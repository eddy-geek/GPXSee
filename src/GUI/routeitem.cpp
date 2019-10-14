#include <QPainter>
#include "data/waypoint.h"
#include "map/map.h"
#include "format.h"
#include "waypointitem.h"
#include "tooltip.h"
#include "routeitem.h"


QString RouteItem::toolTip(Units units) const
{
	ToolTip tt;

	if (!_name.isEmpty())
		tt.insert(tr("Name"), _name);
	if (!_desc.isEmpty())
		tt.insert(tr("Description"), _desc);
	tt.insert(tr("Distance"), Format::distance(path().last().last().distance(),
	  units));
	for (int i = 0; i < _links.size(); i++) {
		const Link &link = _links.at(i);
		if (!link.URL().isEmpty()) {
			tt.insert(tr("Link"), QString("<a href=\"%0\">%1</a>").arg(
			  link.URL(), link.text().isEmpty() ? link.URL() : link.text()));
		}
	}

	return tt.toString();
}

RouteItem::RouteItem(const Route &route, Map *map, QGraphicsItem *parent)
  : PathItem(route.path(), map, parent)
{
	const RouteData &waypoints = route.data();

	_waypoints.resize(waypoints.size());
	for (int i = 0; i < waypoints.size(); i++)
		_waypoints[i] = new WaypointItem(waypoints.at(i), map, this);

	_name = route.name();
	_desc = route.description();
	_links = route.links();
	_coordinatesFormat = DecimalDegrees;
}

void RouteItem::setMap(Map *map)
{
	for (int i = 0; i < _waypoints.count(); i++)
		_waypoints[i]->setMap(map);

	PathItem::setMap(map);
}

void RouteItem::setUnits(Units u)
{
	if (units() == u)
		return;

	for (int i = 0; i < _waypoints.count(); i++)
		_waypoints[i]->setToolTipFormat(u, _coordinatesFormat);

	PathItem::setUnits(u);
}

void RouteItem::setCoordinatesFormat(CoordinatesFormat format)
{
	if (_coordinatesFormat == format)
		return;

	_coordinatesFormat = format;

	for (int i = 0; i < _waypoints.count(); i++)
		_waypoints[i]->setToolTipFormat(units(), _coordinatesFormat);
}

void RouteItem::showWaypoints(bool show)
{
	for (int i = 0; i < _waypoints.count(); i++)
		_waypoints[i]->setVisible(show);
}

void RouteItem::showWaypointLabels(bool show)
{
	for (int i = 0; i < _waypoints.count(); i++)
		_waypoints[i]->showLabel(show);
}
