#include "common/util.h"
#include "objects.h"
#include "attributes.h"
#include "style.h"
#include "mapdata.h"

using namespace ENC;

#define RCNM_VI 110
#define RCNM_VC 120
#define RCNM_VE 130
#define RCNM_VF 140

#define PRIM_P 1
#define PRIM_L 2
#define PRIM_A 3

static QMap<uint,uint> orderMapInit()
{
	QMap<uint,uint> map;

	map.insert(TYPE(LIGHTS), 0);
	map.insert(TYPE(FOGSIG), 0);

	map.insert(TYPE(CGUSTA), 1);
	map.insert(TYPE(RSCSTA), 1);
	map.insert(SUBTYPE(BUAARE, 1), 2);
	map.insert(SUBTYPE(BUAARE, 5), 3);
	map.insert(SUBTYPE(BUAARE, 4), 4);
	map.insert(SUBTYPE(BUAARE, 3), 5);
	map.insert(SUBTYPE(BUAARE, 2), 6);
	map.insert(SUBTYPE(BUAARE, 6), 7);
	map.insert(SUBTYPE(BUAARE, 0), 8);
	map.insert(TYPE(RDOSTA), 9);
	map.insert(TYPE(RADSTA), 10);
	map.insert(TYPE(RTPBCN), 11);
	map.insert(TYPE(BCNISD), 12);
	map.insert(TYPE(BCNLAT), 13);
	map.insert(TYPE(I_BCNLAT), 13);
	map.insert(TYPE(BCNSAW), 14);
	map.insert(TYPE(BCNSPP), 15);
	map.insert(TYPE(BOYCAR), 16);
	map.insert(TYPE(BOYINB), 17);
	map.insert(TYPE(BOYISD), 18);
	map.insert(TYPE(BOYLAT), 19);
	map.insert(TYPE(I_BOYLAT), 19);
	map.insert(TYPE(BOYSAW), 20);
	map.insert(TYPE(BOYSPP), 21);
	map.insert(TYPE(MORFAC), 22);
	map.insert(TYPE(OFSPLF), 23);
	map.insert(TYPE(OBSTRN), 24);
	map.insert(TYPE(WRECKS), 25);
	map.insert(TYPE(UWTROC), 26);
	map.insert(TYPE(WATTUR), 27);
	map.insert(TYPE(CURENT), 28);
	map.insert(TYPE(PILBOP), 29);
	map.insert(TYPE(SISTAT), 30);
	map.insert(TYPE(I_SISTAT), 30);
	map.insert(TYPE(RDOCAL), 31);
	map.insert(TYPE(I_RDOCAL), 31);
	map.insert(TYPE(I_TRNBSN), 32);
	map.insert(TYPE(HRBFAC), 33);
	map.insert(TYPE(I_HRBFAC), 33);
	map.insert(TYPE(PILPNT), 34);
	map.insert(TYPE(ACHBRT), 35);
	map.insert(TYPE(I_ACHBRT), 35);
	map.insert(TYPE(CRANES), 36);
	map.insert(TYPE(I_CRANES), 36);
	map.insert(TYPE(I_WTWGAG), 37);
	map.insert(TYPE(PYLONS), 38);
	map.insert(TYPE(SLCONS), 39);
	map.insert(TYPE(LNDMRK), 40);
	map.insert(TYPE(SILTNK), 41);
	map.insert(TYPE(LNDELV), 42);
	map.insert(TYPE(SMCFAC), 43);
	map.insert(TYPE(BUISGL), 44);

	map.insert(TYPE(I_DISMAR), 0xFFFFFFFE);
	map.insert(TYPE(SOUNDG), 0xFFFFFFFF);

	return map;
}

static QMap<uint,uint> orderMap = orderMapInit();

static uint order(uint type)
{
	uint st = ((type>>16) == BUAARE) ? type : (type & 0xFFFF0000);
	QMap<uint, uint>::const_iterator it = orderMap.find(st);
	return (it == orderMap.constEnd()) ? (type>>16) + 512 : it.value();
}

static void warning(const ISO8211::Field &FRID, uint PRIM)
{
	uint RCID = 0xFFFFFFFF;
	FRID.subfield("RCID", &RCID);

	switch (PRIM) {
		case PRIM_P:
			qWarning("%u: invalid point feature", RCID);
			break;
		case PRIM_L:
			qWarning("%u: invalid line feature", RCID);
			break;
		case PRIM_A:
			qWarning("%u: invalid area feature", RCID);
			break;
	}
}

static void pointBounds(const Coordinates &c, double min[2], double max[2])
{
	min[0] = c.lon();
	min[1] = c.lat();
	max[0] = c.lon();
	max[1] = c.lat();
}

static void rectcBounds(const RectC &rect, double min[2], double max[2])
{
	min[0] = rect.left();
	min[1] = rect.bottom();
	max[0] = rect.right();
	max[1] = rect.top();
}

static bool parseNAME(const ISO8211::Field *f, quint8 *type, quint32 *id,
  int idx = 0)
{
	QByteArray ba(f->data().at(idx).at(0).toByteArray());
	if (ba.size() != 5)
		return false;

	*type = (quint8)(*ba.constData());
	*id = UINT32(ba.constData() + 1);

	return true;
}

static const ISO8211::Field *SGXD(const ISO8211::Record &r)
{
	const ISO8211::Field *f;

	if ((f = r.field("SG2D")))
		return f;
	else if ((f = r.field("SG3D")))
		return f;
	else
		return 0;
}

static bool pointCb(MapData::Point *point, void *context)
{
	QList<MapData::Point*> *points = (QList<MapData::Point*>*)context;
	points->append(point);
	return true;
}

static bool lineCb(MapData::Line *line, void *context)
{
	QList<MapData::Line*> *lines = (QList<MapData::Line*>*)context;
	lines->append(line);
	return true;
}

static bool polygonCb(MapData::Poly *polygon, void *context)
{
	QList<MapData::Poly*> *polygons = (QList<MapData::Poly*>*)context;
	polygons->append(polygon);
	return true;
}

static Coordinates coordinates(int x, int y, uint COMF)
{
	return Coordinates(x / (double)COMF, y / (double)COMF);
}

static Coordinates point(const ISO8211::Record &r, uint COMF)
{
	const ISO8211::Field *f = SGXD(r);
	if (!f)
		return Coordinates();

	int y = f->data().at(0).at(0).toInt();
	int x = f->data().at(0).at(1).toInt();

	return coordinates(x, y, COMF);
}

static uint depthLevel(const QByteArray &ba)
{
	double minDepth = ba.isEmpty() ? -1 : ba.toDouble();

	if (minDepth < 0)
		return 0;
	else if (minDepth < 2)
		return 1;
	else if (minDepth < 5)
		return 2;
	else if (minDepth < 10)
		return 3;
	else if (minDepth < 20)
		return 4;
	else if (minDepth < 50)
		return 5;
	else
		return 6;
}

static QString hUnits(uint type)
{
	switch (type) {
		case 1:
			return "m";
		case 2:
			return "ft";
		case 3:
			return "km";
		case 4:
			return "hm";
		case 5:
			return "mi";
		case 6:
			return "nm";
		default:
			return QString();
	}
}

static QString sistat(uint type)
{
	switch (type) {
		case 1:
			return "SS (Port Control)";
		case 3:
			return "SS (INT)";
		case 6:
			return "SS (Lock)";
		case 8:
			return "SS (Bridge)";
		default:
			return "SS";
	}
}

MapData::Point::Point(uint type, const Coordinates &c, const QString &label,
  const QVector<QByteArray> &params) : _type(type), _pos(c), _label(label)
{
	uint hash = (uint)qHash(QPair<double,double>(c.lon(), c.lat()));
	_id = ((quint64)order(type))<<32 | hash;

	if (type>>16 == I_DISMAR && params.size()) {
		_label = hUnits((type>>8)&0xFF) + " " + QString::fromLatin1(params.at(0));
		_type = SUBTYPE(I_DISMAR, type & 0xFF);
	} else if ((type>>16 == I_RDOCAL || type>>16 == RDOCAL) && params.size() > 1) {
		if (!params.at(1).isEmpty())
			_label = QString("VHF ") + QString::fromLatin1(params.at(1));
		_param = QVariant(params.at(0).toDouble());
	} else if (type>>16 == CURENT && params.size() > 1) {
		if (!params.at(1).isEmpty())
			_label = QString::fromLatin1(params.at(1))
			  + QString::fromUtf8("\xE2\x80\x89kt");
		_param = QVariant(params.at(0).toDouble());
	} else if (type>>16 == I_SISTAT || type>>16 == SISTAT) {
		if (_label.isEmpty())
			_label = sistat(type & 0xFF);
		_type = TYPE(SISTAT);
	} else if (type>>16 == LNDELV && params.size()) {
		if (_label.isEmpty())
			_label = QString::fromLatin1(params.at(0))
			  + QString::fromUtf8("\xE2\x80\x89m");
		else
			_label += "\n(" + QString::fromLatin1(params.at(0))
			  + "\xE2\x80\x89m)";
	}
}

MapData::Poly::Poly(uint type, const Polygon &path,
  const QVector<QByteArray> &params) : _type(type), _path(path)
{
	if (type == TYPE(DEPARE) && params.size())
		_type = SUBTYPE(DEPARE, depthLevel(params.at(0)));
	else if (type == TYPE(TSSLPT) && params.size())
		_param = QVariant(params.at(0).toDouble());
}

MapData::Line::Line(uint type, const QVector<Coordinates> &path,
  const QString &label, const QVector<QByteArray> &params)
  : _type(type), _path(path), _label(label)
{
	if ((type == TYPE(DEPCNT) || type == TYPE(LNDELV)) && params.size())
		_label = QString::fromLatin1(params.at(0));
}

RectC MapData::Line::bounds() const
{
	RectC b;

	for (int i = 0; i < _path.size(); i++)
		b = b.united(_path.at(i));

	return b;
}

QVector<MapData::Sounding> MapData::soundings(const ISO8211::Record &r,
  uint COMF, uint SOMF)
{
	QVector<Sounding> s;
	const ISO8211::Field *f = r.field("SG3D");
	if (!f)
		return QVector<Sounding>();

	s.reserve(f->data().size());
	for (int i = 0; i < f->data().size(); i++) {
		int y = f->data().at(i).at(0).toInt();
		int x = f->data().at(i).at(1).toInt();
		int z = f->data().at(i).at(2).toInt();
		s.append(Sounding(coordinates(x, y, COMF), z / (double)SOMF));
	}

	return s;
}

QVector<MapData::Sounding> MapData::soundingGeometry(const ISO8211::Record &r,
  const RecordMap &vi, const RecordMap &vc, uint COMF, uint SOMF)
{
	quint8 type;
	quint32 id;
	RecordMapIterator it;

	const ISO8211::Field *FSPT = r.field("FSPT");
	if (!FSPT || FSPT->data().at(0).size() != 4)
		return QVector<Sounding>();

	if (!parseNAME(FSPT, &type, &id))
		return QVector<Sounding>();

	if (type == RCNM_VI) {
		it = vi.find(id);
		if (it == vi.constEnd())
			return QVector<Sounding>();
	} else if (type == RCNM_VC) {
		it = vc.find(id);
		if (it == vc.constEnd())
			return QVector<Sounding>();
	} else
		return QVector<Sounding>();

	return soundings(it.value(), COMF, SOMF);
}

Coordinates MapData::pointGeometry(const ISO8211::Record &r,
  const RecordMap &vi, const RecordMap &vc, uint COMF)
{
	quint8 type;
	quint32 id;
	RecordMapIterator it;

	const ISO8211::Field *FSPT = r.field("FSPT");
	if (!FSPT || FSPT->data().at(0).size() != 4)
		return Coordinates();

	if (!parseNAME(FSPT, &type, &id))
		return Coordinates();

	if (type == RCNM_VI) {
		it = vi.find(id);
		if (it == vi.constEnd())
			return Coordinates();
	} else if (type == RCNM_VC) {
		it = vc.find(id);
		if (it == vc.constEnd())
			return Coordinates();
	} else
		return Coordinates();

	return point(it.value(), COMF);
}

QVector<Coordinates> MapData::lineGeometry(const ISO8211::Record &r,
  const RecordMap &vc, const RecordMap &ve, uint COMF)
{
	QVector<Coordinates> path;
	Coordinates c[2];
	uint ORNT;
	quint8 type;
	quint32 id;

	const ISO8211::Field *FSPT = r.field("FSPT");
	if (!FSPT || FSPT->data().at(0).size() != 4)
		return QVector<Coordinates>();

	for (int i = 0; i < FSPT->data().size(); i++) {
		if (!parseNAME(FSPT, &type, &id, i) || type != RCNM_VE)
			return QVector<Coordinates>();
		ORNT = FSPT->data().at(i).at(1).toUInt();

		RecordMapIterator it = ve.find(id);
		if (it == ve.constEnd())
			return QVector<Coordinates>();
		const ISO8211::Record &FRID = it.value();
		const ISO8211::Field *VRPT = FRID.field("VRPT");
		if (!VRPT || VRPT->data().size() != 2)
			return QVector<Coordinates>();

		for (int j = 0; j < 2; j++) {
			if (!parseNAME(VRPT, &type, &id, j) || type != RCNM_VC)
				return QVector<Coordinates>();

			RecordMapIterator jt = vc.find(id);
			if (jt == vc.constEnd())
				return QVector<Coordinates>();
			c[j] = point(jt.value(), COMF);
			if (c[j].isNull())
				return QVector<Coordinates>();
		}

		const ISO8211::Field *vertexes = SGXD(FRID);
		if (ORNT == 2) {
			path.append(c[1]);
			if (vertexes) {
				for (int j = vertexes->data().size() - 1; j >= 0; j--) {
					const QVector<QVariant> &cv = vertexes->data().at(j);
					path.append(coordinates(cv.at(1).toInt(), cv.at(0).toInt(),
					  COMF));
				}
			}
			path.append(c[0]);
		} else {
			path.append(c[0]);
			if (vertexes) {
				for (int j = 0; j < vertexes->data().size(); j++) {
					const QVector<QVariant> &cv = vertexes->data().at(j);
					path.append(coordinates(cv.at(1).toInt(), cv.at(0).toInt(),
					  COMF));
				}
			}
			path.append(c[1]);
		}
	}

	return path;
}

Polygon MapData::polyGeometry(const ISO8211::Record &r, const RecordMap &vc,
  const RecordMap &ve, uint COMF)
{
	Polygon path;
	QVector<Coordinates> v;
	Coordinates c[2];
	uint ORNT, USAG;
	quint8 type;
	quint32 id;

	const ISO8211::Field *FSPT = r.field("FSPT");
	if (!FSPT || FSPT->data().at(0).size() != 4)
		return Polygon();

	for (int i = 0; i < FSPT->data().size(); i++) {
		if (!parseNAME(FSPT, &type, &id, i) || type != RCNM_VE)
			return Polygon();
		ORNT = FSPT->data().at(i).at(1).toUInt();
		USAG = FSPT->data().at(i).at(2).toUInt();

		if (USAG == 2 && path.isEmpty()) {
			path.append(v);
			v.clear();
		}

		RecordMapIterator it = ve.find(id);
		if (it == ve.constEnd())
			return Polygon();
		const ISO8211::Record &FRID = it.value();
		const ISO8211::Field *VRPT = FRID.field("VRPT");
		if (!VRPT || VRPT->data().size() != 2)
			return Polygon();

		for (int j = 0; j < 2; j++) {
			if (!parseNAME(VRPT, &type, &id, j) || type != RCNM_VC)
				return Polygon();

			RecordMapIterator jt = vc.find(id);
			if (jt == vc.constEnd())
				return Polygon();
			c[j] = point(jt.value(), COMF);
			if (c[j].isNull())
				return Polygon();
		}

		const ISO8211::Field *vertexes = SGXD(FRID);
		if (ORNT == 2) {
			v.append(c[1]);
			if (vertexes) {
				for (int j = vertexes->data().size() - 1; j >= 0; j--) {
					const QVector<QVariant> &cv = vertexes->data().at(j);
					v.append(coordinates(cv.at(1).toInt(), cv.at(0).toInt(),
					  COMF));
				}
			}
			v.append(c[0]);
		} else {
			v.append(c[0]);
			if (vertexes) {
				for (int j = 0; j < vertexes->data().size(); j++) {
					const QVector<QVariant> &cv = vertexes->data().at(j);
					v.append(coordinates(cv.at(1).toInt(), cv.at(0).toInt(),
					  COMF));
				}
			}
			v.append(c[1]);
		}

		if (USAG == 2 && v.first() == v.last()) {
			path.append(v);
			v.clear();
		}
	}

	if (!v.isEmpty())
		path.append(v);

	return path;
}

MapData::Attr MapData::pointAttr(const ISO8211::Record &r, uint OBJL)
{
	QString label;
	QVector<QByteArray> params(2);
	uint subtype = 0;

	const ISO8211::Field *ATTF = r.field("ATTF");
	if (!(ATTF && ATTF->data().at(0).size() == 2))
		return Attr();

	for (int i = 0; i < ATTF->data().size(); i++) {
		const QVector<QVariant> &av = ATTF->data().at(i);
		uint key = av.at(0).toUInt();

		if (key == OBJNAM)
			label = QString::fromLatin1(av.at(1).toByteArray());

		if ((OBJL == HRBFAC && key == CATHAF)
		  || (OBJL == I_HRBFAC && key == I_CATHAF)
		  || (OBJL == LNDMRK && key == CATLMK)
		  || (OBJL == WRECKS && key == CATWRK)
		  || (OBJL == MORFAC && key == CATMOR)
		  || (OBJL == UWTROC && key == WATLEV)
		  || (OBJL == BUAARE && key == CATBUA)
		  || (OBJL == SMCFAC && key == CATSCF)
		  || (OBJL == BUISGL && key == FUNCTN)
		  || (OBJL == WATTUR && key == CATWAT)
		  || (OBJL == SISTAT && key == CATSIT)
		  || (OBJL == I_SISTAT && key == I_CATSIT))
			subtype = av.at(1).toByteArray().toUInt();
		else if (OBJL == I_DISMAR && key == CATDIS)
			subtype |= av.at(1).toByteArray().toUInt();
		else if (OBJL == I_DISMAR && key == I_HUNITS)
			subtype |= av.at(1).toByteArray().toUInt() << 8;

		if ((OBJL == I_DISMAR && key == I_WTWDIS)
		  || (OBJL == RDOCAL && key == ORIENT)
		  || (OBJL == I_RDOCAL && key == ORIENT)
		  || (OBJL == CURENT && key == ORIENT)
		  || (OBJL == LNDELV && key == ELEVAT))
			params[0] = av.at(1).toByteArray();
		if ((OBJL == I_RDOCAL && key == COMCHA)
		  || (OBJL == RDOCAL && key == COMCHA)
		  || (OBJL == CURENT && key == CURVEL))
			params[1] = av.at(1).toByteArray();
	}

	return Attr(subtype, label, params);
}

MapData::Attr MapData::lineAttr(const ISO8211::Record &r, uint OBJL)
{
	QString label;
	QVector<QByteArray> params(1);
	uint subtype = 0;

	const ISO8211::Field *ATTF = r.field("ATTF");
	if (!(ATTF && ATTF->data().at(0).size() == 2))
		return Attr();

	for (int i = 0; i < ATTF->data().size(); i++) {
		const QVector<QVariant> &av = ATTF->data().at(i);
		uint key = av.at(0).toUInt();

		if (key == OBJNAM)
			label = QString::fromLatin1(av.at(1).toByteArray());

		if ((OBJL == RECTRC || OBJL == RCRTCL) && key == CATTRK)
			subtype = av.at(1).toByteArray().toUInt();

		if ((OBJL == DEPCNT && key == VALDCO)
		  || (OBJL == LNDELV && key == ELEVAT))
			params[0] = av.at(1).toByteArray();
	}

	return Attr(subtype, label, params);
}

MapData::Attr MapData::polyAttr(const ISO8211::Record &r, uint OBJL)
{
	QString label;
	QVector<QByteArray> params(1);
	uint subtype = 0;

	const ISO8211::Field *ATTF = r.field("ATTF");
	if (!(ATTF && ATTF->data().at(0).size() == 2))
		return Attr();

	for (int i = 0; i < ATTF->data().size(); i++) {
		const QVector<QVariant> &av = ATTF->data().at(i);
		uint key = av.at(0).toUInt();

		if ((OBJL == RESARE && key == CATREA)
		  || (OBJL == I_RESARE && key == CATREA)
		  || (OBJL == ACHARE && key == CATACH)
		  || (OBJL == I_ACHARE && key == I_CATACH)
		  || (OBJL == HRBFAC && key == CATHAF)
		  || (OBJL == MARKUL && key == CATMFA)
		  || (OBJL == I_BERTHS && key == I_CATBRT))
			subtype = av.at(1).toByteArray().toUInt();
		else if ((OBJL == RESARE && key == RESTRN)
		  || (OBJL == I_RESARE && key == I_RESTRN)) {
			if (av.at(1).toByteArray().toUInt() == 1)
				subtype = 2;
			if (av.at(1).toByteArray().toUInt() == 7)
				subtype = 17;
		}

		if ((OBJL == TSSLPT && key == ORIENT)
		  || (OBJL == DEPARE && key == DRVAL1))
			params[0] = av.at(1).toByteArray();
	}

	return Attr(subtype, label, params);
}

MapData::Point *MapData::pointObject(const Sounding &s)
{
	return new Point(TYPE(SOUNDG), s.c, QString::number(s.depth),
	  QVector<QByteArray>());
}

MapData::Point *MapData::pointObject(const ISO8211::Record &r,
  const RecordMap &vi, const RecordMap &vc, uint COMF, uint OBJL)
{
	Coordinates c(pointGeometry(r, vi, vc, COMF));
	Attr attr(pointAttr(r, OBJL));

	return (c.isNull() ? 0 : new Point(SUBTYPE(OBJL,attr.subtype()), c,
	  attr.label(), attr.params()));
}

MapData::Line *MapData::lineObject(const ISO8211::Record &r,
  const RecordMap &vc, const RecordMap &ve, uint COMF, uint OBJL)
{
	QVector<Coordinates> path(lineGeometry(r, vc, ve, COMF));
	Attr attr(lineAttr(r, OBJL));

	return (path.isEmpty() ? 0 : new Line(SUBTYPE(OBJL, attr.subtype()), path,
	  attr.label(), attr.params()));
}

MapData::Poly *MapData::polyObject(const ISO8211::Record &r,
  const RecordMap &vc, const RecordMap &ve, uint COMF, uint OBJL)
{
	Polygon path(polyGeometry(r, vc, ve, COMF));
	Attr attr(polyAttr(r, OBJL));

	return (path.isEmpty() ? 0 : new Poly(SUBTYPE(OBJL, attr.subtype()), path,
	  attr.params()));
}

bool MapData::processRecord(const ISO8211::Record &record,
  QVector<ISO8211::Record> &rv, uint &COMF, QString &name)
{
	if (record.size() < 2)
		return false;

	const ISO8211::Field &f = record.at(1);
	const QByteArray &ba = f.tag();

	if (ba == "VRID") {
		rv.append(record);
	} else if (ba == "DSID") {
		QByteArray DSNM;
		if (!f.subfield("DSNM", &DSNM))
			return false;
		name = DSNM;
	} else if (ba == "DSPM") {
		if (!f.subfield("COMF", &COMF))
			return false;
	}

	return true;
}

bool MapData::processRecord(const ISO8211::Record &record,
  QVector<ISO8211::Record> &fe, RecordMap &vi, RecordMap &vc, RecordMap &ve,
  RecordMap &vf, uint &COMF, uint &SOMF)
{
	if (record.size() < 2)
		return false;

	const ISO8211::Field &f = record.at(1);
	const QByteArray &ba = f.tag();

	if (ba == "VRID") {
		if (f.data().at(0).size() < 2)
			return false;
		int RCNM = f.data().at(0).at(0).toInt();
		uint RCID = f.data().at(0).at(1).toUInt();

		switch (RCNM) {
			case RCNM_VI:
				vi.insert(RCID, record);
				break;
			case RCNM_VC:
				vc.insert(RCID, record);
				break;
			case RCNM_VE:
				ve.insert(RCID, record);
				break;
			case RCNM_VF:
				vf.insert(RCID, record);
				break;
			default:
				return false;
		}
	} else if (ba == "FRID") {
		fe.append(record);
	} else if (ba == "DSPM") {
		if (!(f.subfield("COMF", &COMF) && f.subfield("SOMF", &SOMF)))
			return false;
	}

	return true;
}

bool MapData::bounds(const ISO8211::Record &record, Rect &rect)
{
	bool xok, yok;
	// edge geometries can be empty!
	const ISO8211::Field *f = SGXD(record);
	if (!f)
		return true;

	for (int i = 0; i < f->data().size(); i++) {
		const QVector<QVariant> &c = f->data().at(i);
		rect.unite(c.at(1).toInt(&xok), c.at(0).toInt(&yok));
		if (!(xok && yok))
			return false;
	}

	return true;
}

bool MapData::bounds(const QVector<ISO8211::Record> &gv, Rect &b)
{
	Rect r;

	for (int i = 0; i < gv.size(); i++) {
		if (!bounds(gv.at(i), r))
			return false;
		b |= r;
	}

	return true;
}

MapData::MapData(const QString &path): _fileName(path)
{
	QVector<ISO8211::Record> gv;
	ISO8211 ddf(_fileName);
	ISO8211::Record record;
	uint COMF = 1;

	if (!ddf.readDDR()) {
		_errorString = ddf.errorString();
		return;
	}
	while (ddf.readRecord(record)) {
		if (!processRecord(record, gv, COMF, _name)) {
			_errorString = "Invalid S-57 record";
			return;
		}
	}
	if (!ddf.errorString().isNull()) {
		_errorString = ddf.errorString();
		return;
	}

	Rect b;
	if (!bounds(gv, b)) {
		_errorString = "Error fetching geometries bounds";
		return;
	}
	RectC br(Coordinates(b.minX() / (double)COMF, b.maxY() / (double)COMF),
	  Coordinates(b.maxX() / (double)COMF, b.minY() / (double)COMF));
	if (!br.isValid())
		_errorString = "Invalid geometries bounds";
	else
		_bounds = br;
}

MapData::~MapData()
{
	LineTree::Iterator lit;
	for (_lines.GetFirst(lit); !_lines.IsNull(lit); _lines.GetNext(lit))
		delete _lines.GetAt(lit);

	PolygonTree::Iterator ait;
	for (_areas.GetFirst(ait); !_areas.IsNull(ait); _areas.GetNext(ait))
		delete _areas.GetAt(ait);

	PointTree::Iterator pit;
	for (_points.GetFirst(pit); !_points.IsNull(pit); _points.GetNext(pit))
		delete _points.GetAt(pit);
}

void MapData::load()
{
	RecordMap vi, vc, ve, vf;
	QVector<ISO8211::Record> fe;
	ISO8211 ddf(_fileName);
	ISO8211::Record record;
	uint PRIM, OBJL, COMF = 1, SOMF = 1;
	Poly *poly;
	Line *line;
	Point *point;
	double min[2], max[2];


	if (!ddf.readDDR())
		return;
	while (ddf.readRecord(record))
		if (!processRecord(record, fe, vi, vc, ve, vf, COMF, SOMF))
			qWarning("Invalid S-57 record");

	for (int i = 0; i < fe.size(); i++) {
		const ISO8211::Record &r = fe.at(i);
		const ISO8211::Field &f = r.at(1);

		if (f.data().at(0).size() < 5)
			continue;
		PRIM = f.data().at(0).at(2).toUInt();
		OBJL = f.data().at(0).at(4).toUInt();

		switch (PRIM) {
			case PRIM_P:
				if (OBJL == SOUNDG) {
					QVector<Sounding> s(soundingGeometry(r, vi, vc, COMF, SOMF));
					for (int i = 0; i < s.size(); i++) {
						point = pointObject(s.at(i));
						pointBounds(point->pos(), min, max);
						_points.Insert(min, max, point);
					}
				} else {
					if ((point = pointObject(r, vi, vc, COMF, OBJL))) {
						pointBounds(point->pos(), min, max);
						_points.Insert(min, max, point);
					} else
						warning(f, PRIM);
				}
				break;
			case PRIM_L:
				if ((line = lineObject(r, vc, ve, COMF, OBJL))) {
					rectcBounds(line->bounds(), min, max);
					_lines.Insert(min, max, line);
				} else
					warning(f, PRIM);
				break;
			case PRIM_A:
				if ((poly = polyObject(r, vc, ve, COMF, OBJL))) {
					rectcBounds(poly->bounds(), min, max);
					_areas.Insert(min, max, poly);
				} else
					warning(f, PRIM);
				break;
		}
	}
}

void MapData::clear()
{
	LineTree::Iterator lit;
	for (_lines.GetFirst(lit); !_lines.IsNull(lit); _lines.GetNext(lit))
		delete _lines.GetAt(lit);
	_lines.RemoveAll();

	PolygonTree::Iterator ait;
	for (_areas.GetFirst(ait); !_areas.IsNull(ait); _areas.GetNext(ait))
		delete _areas.GetAt(ait);
	_areas.RemoveAll();

	PointTree::Iterator pit;
	for (_points.GetFirst(pit); !_points.IsNull(pit); _points.GetNext(pit))
		delete _points.GetAt(pit);
	_points.RemoveAll();
}

void MapData::points(const RectC &rect, QList<Point*> *points) const
{
	double min[2], max[2];

	rectcBounds(rect, min, max);
	_points.Search(min, max, pointCb, points);
}

void MapData::lines(const RectC &rect, QList<Line*> *lines) const
{
	double min[2], max[2];

	rectcBounds(rect, min, max);
	_lines.Search(min, max, lineCb, lines);
}

void MapData::polygons(const RectC &rect, QList<Poly*> *polygons) const
{
	double min[2], max[2];

	rectcBounds(rect, min, max);
	_areas.Search(min, max, polygonCb, polygons);
}

Range MapData::zooms() const
{
	double size = qMin(_bounds.width(), _bounds.height());

	if (size > 180)
		return Range(0, 10);
	else if (size > 90)
		return Range(1, 11);
	else if (size > 45)
		return Range(2, 12);
	else if (size > 22.5)
		return Range(3, 13);
	else if (size > 11.25)
		return Range(4, 14);
	else if (size > 5.625)
		return Range(5, 15);
	else if (size > 2.813)
		return Range(6, 16);
	else if (size > 1.406)
		return Range(7, 17);
	else if (size > 0.703)
		return Range(8, 18);
	else if (size > 0.352)
		return Range(9, 19);
	else if (size > 0.176)
		return Range(10, 20);
	else if (size > 0.088)
		return Range(11, 20);
	else if (size > 0.044)
		return Range(12, 20);
	else if (size > 0.022)
		return Range(13, 20);
	else if (size > 0.011)
		return Range(14, 20);
	else
		return Range(15, 20);
}
