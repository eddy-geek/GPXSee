#include <cmath>
#include <QtMath>
#include "jls.h"

using namespace IMG;

static const quint8 Z[] = {
	8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

static const quint8 J[] = {
	0,  0,  0,  0,  1,  1,  1,  1,  2,  2,  2,  2,  3,  3,  3,  3,
	4,  4,  5,  5,  6,  6,  7,  7,  8,  9, 10, 11, 12, 13, 14, 15
};

JLS::JLS(quint16 maxval, quint16 near)
{
	_maxval = maxval;
	_near = near;

	_range = ((_maxval + _near * 2) / (_near * 2 + 1)) + 1;
	_qbpp = qCeil(log2(_range));
	quint8 bpp = qMax(2, qCeil(log2(_maxval + 1)));
	quint8 LIMIT = 2 * (bpp + qMax((quint8)8, bpp));
	_limit = LIMIT - _qbpp - 1;
}

bool JLS::processRunMode(BitStream &bs, Context &ctx, quint16 col,
  quint16 &samples) const
{
	quint8 z;
	quint16 cnt = 0;

	while (true) {
		if ((qint32)bs.value() < 0) {
			z = Z[(bs.value() >> 0x18) ^ 0xff];

			for (quint8 i = 0; i < z; i++) {
				cnt = cnt + ctx.rg;

				if (cnt <= col && ctx.runIndex < 31) {
					ctx.runIndex++;
					ctx.rk = J[ctx.runIndex];
					ctx.rg = 1U << ctx.rk;
				}

				if (cnt >= col) {
					if (!bs.read(i + 1))
						return false;

					samples = col;
					return true;
				}
			}
		} else
			z = 0;

		if (z != 8) {
			if (!bs.read(z + 1))
				return false;

			if (ctx.rk) {
				samples = (bs.value() >> (32 - ctx.rk)) + cnt;
				if (!bs.read(ctx.rk))
					return false;
			} else
				samples = cnt;

			ctx.lrk = ctx.rk + 1;
			if (ctx.runIndex != 0) {
				ctx.runIndex--;
				ctx.rk = J[ctx.runIndex];
				ctx.rg = 1U << ctx.rk;
			}

			return true;
		}

		if (!bs.read(8))
			return false;
	}
}

bool JLS::decodeError(BitStream &bs, quint8 limit, quint8 k,
  uint &MErrval) const
{
	quint8 cnt = 0;
	MErrval = 0;

	while ((int)bs.value() >= 0) {
		cnt = Z[bs.value() >> 0x18];
		MErrval += cnt;
		if (bs.value() >> 0x18 != 0)
			break;

		if (!bs.read(8))
			return false;

		cnt = 0;
	}

	if (!bs.read(cnt + 1))
		return false;

	if (MErrval < limit) {
		if (k != 0) {
			MErrval = (bs.value() >> (0x20 - k)) + (MErrval << k);
			if (!bs.read(k))
				return false;
		}
	} else {
		MErrval = (bs.value() >> (0x20 - _qbpp)) + 1;
		if (!bs.read(_qbpp))
			return false;
	}

	return true;
}

bool JLS::readLine(BitStream &bs, Context &ctx) const
{
	quint8 ictx, rctx;
	quint8 k;
	uint MErrval;
	int Errval;
	int Rx;
	int Ra = ctx.last[1];
	int Rb = ctx.last[1];
	int Rc = ctx.last[0];
	uint col = 1;

	*ctx.current = ctx.last[1];

	do {
		if (abs(Rb - Ra) > _near) {
			int Px = Ra + Rb - Rc;
			if (Px < 0)
				Px = 0;
			else if (Px > _maxval)
				Px = _maxval;

			for (k = 0; ctx.n[1] << k < ctx.a[1]; k++)
				;

			if (!decodeError(bs, _limit, k, MErrval))
				return false;

			int mes, meh;
			if (MErrval & 1) {
				meh = (MErrval + 1) >> 1;
				mes = -meh;
			} else {
				meh = MErrval >> 1;
				mes = meh;
			}
			if ((_near == 0) && (k == 0) && (ctx.b[1] * 2 <= -ctx.n[1])) {
				meh = mes + 1;
				mes = -mes - 1;
				if (MErrval & 1)
					meh = mes;
			} else
				mes = mes * (_near * 2 + 1);

			Errval = (Ra < Rb) ? mes : -mes;
			Rx = Px + Errval;

			if (Rx < -_near)
				Rx += (_near * 2 + 1) * _range;
			else if (Rx > _maxval + _near)
				Rx -= (_near * 2 + 1) * _range;

			if (Rx < 0)
				Rx = 0;
			if (Rx > _maxval)
				Rx = _maxval;

			ctx.a[1] = ctx.a[1] + meh;
			ctx.b[1] = ctx.b[1] + mes;
			if (ctx.n[1] == 0x40) {
				ctx.a[1] = ctx.a[1] >> 1;
				if (ctx.b[1] >= 0)
					ctx.b[1] = ctx.b[1] >> 1;
				else
					ctx.b[1] = -((1 - ctx.b[1]) >> 1);
				ctx.n[1] = 0x21;
			} else
				ctx.n[1] = ctx.n[1] + 1;

			if (ctx.b[1] <= -ctx.n[1]) {
				ctx.b[1] = ctx.b[1] + ctx.n[1];
				if (ctx.b[1] <= -ctx.n[1])
					ctx.b[1] = 1 - ctx.n[1];
			} else if (ctx.b[1] > 0) {
				ctx.b[1] = ctx.b[1] - ctx.n[1];
				if (ctx.b[1] > 0)
					ctx.b[1] = 0;
			}

			Rc = Rb;
			Rb = ctx.last[col + 1];
		} else {
			quint16 samples;
			if (!processRunMode(bs, ctx, ctx.w - col + 1, samples))
				return false;

			if (samples != 0) {
				for (int i = 0; i < samples; i++) {
					if (col > ctx.w)
						return false;
					ctx.current[col] = Ra;
					col++;
				}

				if (col > ctx.w)
					break;

				Rc = ctx.last[col];
				Rb = ctx.last[col + 1];
			} else {
				Rc = Rb;
				Rb = ctx.last[col + 1];
			}

			rctx = (abs(Rc - Ra) <= _near);
			quint16 TEMP = ctx.a[rctx + 2];
			if (rctx)
				TEMP += ctx.n[rctx + 2] >> 1;
			ictx = rctx | 2;

			for (k = 0; ctx.n[rctx + 2] << k < TEMP; k++)
				;

			if (!decodeError(bs, _limit - ctx.lrk, k, MErrval))
				return false;

			quint16 s = ((k == 0) && (rctx || MErrval)) ?
			  (ctx.b[ictx] * 2 < ctx.n[ictx]) : 0;

			Errval = MErrval + rctx + s;
			int evh;
			if ((Errval & 1) == 0) {
				Errval = Errval / 2;
				evh = Errval;
			} else {
				Errval = s - ((Errval + 1) >> 1);
				evh = -Errval;
				ctx.b[ictx] = ctx.b[ictx] + 1;
			}

			Errval *= (_near * 2 + 1);
			if (!rctx) {
				if (Ra == Rc)
					return false;

				if (Ra < Rc)
					Rx = Rc + Errval;
				else
					Rx = Rc - Errval;
			} else
				Rx = Ra + Errval;

			if (Rx < -_near)
				Rx += (_near * 2 + 1) * _range;
			else if (Rx > _maxval + _near)
				Rx -= (_near * 2 + 1) * _range;

			if (Rx < 0)
				Rx = 0;
			if (Rx > _maxval)
				Rx = _maxval;

			ctx.a[ictx] = ctx.a[ictx] + (evh - rctx);
			if (ctx.n[ictx] == 0x40) {
				ctx.a[ictx] = ctx.a[ictx] >> 1;
				if (ctx.b[ictx] >= 0)
					ctx.b[ictx] = ctx.b[ictx] >> 1;
				else
					ctx.b[ictx] = -((1 - ctx.b[ictx]) >> 1);
				ctx.n[ictx] = 0x21;
			} else
				ctx.n[ictx] = ctx.n[ictx] + 1;
		}

		ctx.current[col] = Rx;

		Ra = Rx;
		col = col + 1;
	} while (col <= ctx.w);

	return true;
}

bool JLS::decode(const SubFile *file, SubFile::Handle &hdl,
  Matrix<qint16> &img) const
{
	Context ctx(img.w(), _range);
	BitStream bs(file, hdl);

	if (!bs.init())
		return false;

	for (int i = 0; i < img.h(); i++) {
		if (!readLine(bs, ctx))
			return false;

		memcpy(img.row(i), ctx.current + 1, img.w() * sizeof(quint16));

		quint16 *tmp = ctx.last;
		ctx.last = ctx.current;
		ctx.current = tmp;
	}

	return true;
}
