#include <QFileInfo>
#include "common/programpaths.h"
#include "vectortile.h"
#include "style.h"
#include "mapdata.h"


using namespace IMG;

#define CACHED_SUBDIVS_COUNT 2048 // ~32MB for both caches together

bool MapData::polyCb(VectorTile *tile, void *context)
{
	PolyCTX *ctx = (PolyCTX*)context;
	tile->polys(ctx->rect, ctx->zoom, ctx->polygons, ctx->lines,
	  ctx->polyCache, ctx->lock);
	return true;
}

bool MapData::pointCb(VectorTile *tile, void *context)
{
	PointCTX *ctx = (PointCTX*)context;
	tile->points(ctx->rect, ctx->zoom, ctx->points, ctx->pointCache, ctx->lock);
	return true;
}


MapData::MapData(const QString &fileName)
  : _fileName(fileName), _typ(0), _style(0), _valid(false)
{
	_polyCache.setMaxCost(CACHED_SUBDIVS_COUNT);
	_pointCache.setMaxCost(CACHED_SUBDIVS_COUNT);
}

MapData::~MapData()
{
	TileTree::Iterator it;
	for (_tileTree.GetFirst(it); !_tileTree.IsNull(it); _tileTree.GetNext(it))
		delete _tileTree.GetAt(it);

	delete _typ;
	delete _style;
}

void MapData::polys(const RectC &rect, int bits, QList<Poly> *polygons,
  QList<Poly> *lines)
{
	PolyCTX ctx(rect, zoom(bits), polygons, lines, &_polyCache, &_lock);
	double min[2], max[2];

	min[0] = rect.left();
	min[1] = rect.bottom();
	max[0] = rect.right();
	max[1] = rect.top();

	_tileTree.Search(min, max, polyCb, &ctx);
}

void MapData::points(const RectC &rect, int bits, QList<Point> *points)
{
	PointCTX ctx(rect, zoom(bits), points, &_pointCache, &_lock);
	double min[2], max[2];

	min[0] = rect.left();
	min[1] = rect.bottom();
	max[0] = rect.right();
	max[1] = rect.top();

	_tileTree.Search(min, max, pointCb, &ctx);
}

void MapData::load()
{
	Q_ASSERT(!_style);

	if (_typ)
		_style = new Style(_typ);
	else {
		QString typFile(ProgramPaths::typFile());
		if (QFileInfo::exists(typFile)) {
			SubFile typ(&typFile);
			_style = new Style(&typ);
		} else
			_style = new Style();
	}
}

void MapData::clear()
{
	TileTree::Iterator it;
	for (_tileTree.GetFirst(it); !_tileTree.IsNull(it); _tileTree.GetNext(it))
		_tileTree.GetAt(it)->clear();

	delete _style;
	_style = 0;

	_polyCache.clear();
	_pointCache.clear();
}

void MapData::computeZooms()
{
	TileTree::Iterator it;
	QSet<Zoom> zooms;

	for (_tileTree.GetFirst(it); !_tileTree.IsNull(it); _tileTree.GetNext(it)) {
		const QVector<Zoom> &z = _tileTree.GetAt(it)->zooms();
		for (int i = 0; i < z.size(); i++)
			zooms.insert(z.at(i));
	}

	if (zooms.isEmpty())
		return;

	_zooms = zooms.values();
	std::sort(_zooms.begin(), _zooms.end());

	bool baseMap = false;
	for (int i = 1; i < _zooms.size(); i++) {
		if (_zooms.at(i).level() > _zooms.at(i-1).level()) {
			baseMap = true;
			break;
		}
	}
	_zoomLevels = Range(baseMap ? _zooms.first().bits()
	  : qMax(0, _zooms.first().bits() - 2), 28);
}

const Zoom &MapData::zoom(int bits) const
{
	int id = 0;

	for (int i = 1; i < _zooms.size(); i++) {
		if (_zooms.at(i).bits() > bits)
			break;
		id++;
	}

	return _zooms.at(id);
}
