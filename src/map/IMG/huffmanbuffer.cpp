#include "rgnfile.h"
#include "huffmanbuffer.h"

bool HuffmanBuffer::load(const RGNFile *rgn, SubFile::Handle &rgnHdl)
{
	quint32 recordSize, recordOffset = rgn->dictOffset();

	for (int i = 0; i <= _id; i++) {
		if (!rgn->seek(rgnHdl, recordOffset))
			return false;
		if (!rgn->readVUInt32(rgnHdl, recordSize))
			return false;
		recordOffset = rgn->pos(rgnHdl) + recordSize;
		if (recordOffset > rgn->dictOffset() + rgn->dictSize())
			return false;
	};

	resize(recordSize);
	return rgn->read(rgnHdl, (quint8*)data(), recordSize);
}
