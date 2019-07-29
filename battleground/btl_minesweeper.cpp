#include "btl_minesweeper.h"
#include "gpk_chrono.h"
#include "gpk_encoding.h"

::gpk::error_t						btl::SMineBack::GetBlast	(::gpk::SCoord2<uint32_t> & out_coord)	const {
	const ::gpk::SCoord2<uint32_t>	gridMetrix = Board.metrics();
	for(uint32_t y = 0; y < gridMetrix.y; ++y) {
		for(uint32_t x = 0; x < gridMetrix.x; ++x) {
			const uint8_t					value						=	Board[y][x].Boom ? 1 : 0;
			if(value > 0) {
				out_coord					= {x, y};
				return 1;
			}
		}
	}
	return 0;
}

#define ASSIGN_GRID_BOOL(value_, total_) {					\
		for(uint32_t y = 0; y < gridMetrix.y; ++y) {		\
			const uint32_t offset = y * gridMetrix.x;		\
			for(uint32_t x = 0; x < gridMetrix.x; ++x) {	\
				uint8_t	value =	value_ ? 1 : 0;				\
				out_Cells[offset + x] = value;				\
				if(value)									\
					total_ += value;						\
			}												\
		}													\
	}

::gpk::error_t						btl::SMineBack::GetMines	(::gpk::view_bit<uint64_t> & out_Cells)	const	{ const ::gpk::SCoord2<uint32_t> gridMetrix = Board.metrics(); uint32_t total = 0; ASSIGN_GRID_BOOL(Board[y][x].Mine, total); return total; }
::gpk::error_t						btl::SMineBack::GetFlags	(::gpk::view_bit<uint64_t> & out_Cells)	const	{ const ::gpk::SCoord2<uint32_t> gridMetrix = Board.metrics(); uint32_t total = 0; ASSIGN_GRID_BOOL(Board[y][x].Flag, total); return total; }
::gpk::error_t						btl::SMineBack::GetHolds	(::gpk::view_bit<uint64_t> & out_Cells)	const	{ const ::gpk::SCoord2<uint32_t> gridMetrix = Board.metrics(); uint32_t total = 0; ASSIGN_GRID_BOOL(Board[y][x].What, total); return total; }
::gpk::error_t						btl::SMineBack::GetHides	(::gpk::view_bit<uint64_t> & out_Cells)	const	{ const ::gpk::SCoord2<uint32_t> gridMetrix = Board.metrics(); uint32_t total = 0; ASSIGN_GRID_BOOL(Board[y][x].Hide, total); return total; }
::gpk::error_t						btl::SMineBack::GetHints	(::gpk::view_grid<uint8_t> & out_Cells)	const	{ const ::gpk::SCoord2<uint32_t> gridMetrix = Board.metrics(); uint32_t total = 0;
	for(int32_t y = 0; y < (int32_t)gridMetrix.y; ++y)
	for(int32_t x = 0; x < (int32_t)gridMetrix.x; ++x) {
		const ::btl::SMineBackCell				cellValue					= Board[y][x];
		const uint8_t							hidden						= cellValue.Hide ? 1 : 0; (void)hidden;
		//if(cellValue.Hide)
		//	continue;
		if(cellValue.Mine)
			continue;
		uint8_t									& out				= out_Cells[y][x]		= 0;
		::gpk::SCoord2<int32_t>					coordToTest			= {};	// actually by making this uint32_t we could easily change all the conditions to be coordToTest.i < gridMetrix.i. However, I'm too lazy to start optimizing what's hardly the bottleneck
		coordToTest							= {x - 1, y - 1	};	if(coordToTest.y >= 0 && coordToTest.x >= 0)										{ if(Board[coordToTest.y][coordToTest.x].Mine) ++out; }
		coordToTest							= {x	, y - 1	};	if(coordToTest.y >= 0)																{ if(Board[coordToTest.y][coordToTest.x].Mine) ++out; }
		coordToTest							= {x + 1, y - 1	};	if(coordToTest.y >= 0 && coordToTest.x < (int32_t)gridMetrix.x)						{ if(Board[coordToTest.y][coordToTest.x].Mine) ++out; }

		coordToTest							= {x - 1, y		};	if(coordToTest.x >= 0)																{ if(Board[coordToTest.y][coordToTest.x].Mine) ++out; }
		coordToTest							= {x + 1, y		};	if(coordToTest.x < (int32_t)gridMetrix.x)											{ if(Board[coordToTest.y][coordToTest.x].Mine) ++out; }

		coordToTest							= {x - 1, y + 1	};	if(coordToTest.y < (int32_t)gridMetrix.y && coordToTest.x >= 0)						{ if(Board[coordToTest.y][coordToTest.x].Mine) ++out; }
		coordToTest							= {x	, y + 1	};	if(coordToTest.y < (int32_t)gridMetrix.y)											{ if(Board[coordToTest.y][coordToTest.x].Mine) ++out; }
		coordToTest							= {x + 1, y + 1	};	if(coordToTest.y < (int32_t)gridMetrix.y && coordToTest.x < (int32_t)gridMetrix.x)	{ if(Board[coordToTest.y][coordToTest.x].Mine) ++out; }
	}
	return total;
}

static	::gpk::error_t				uncoverCell						(::btl::SMineBack & gameState, const ::gpk::SCoord2<int32_t> cell) {
	::gpk::view_grid<::btl::SMineBackCell> board							= gameState.Board.View;
	board[cell.y][cell.x].Hide			= false;

	const ::gpk::SCoord2<uint32_t>			gridMetrix						= board.metrics();
	::gpk::SImage<uint8_t>					hints;
	hints.resize(gridMetrix);
	gameState.GetHints(hints.View);
	if(0 == hints[cell.y][cell.x]) {
		hints.Texels.clear_pointer();
		::gpk::SCoord2<int32_t>					coordToTest						= {};	// actually by making this uint32_t we could easily change all the conditions to be coordToTest.i < gridMetrix.i. However, I'm too lazy to start optimizing what's hardly the bottleneck
		coordToTest	= {cell.x - 1, cell.y - 1	};	if(coordToTest.y >= 0 && coordToTest.x >= 0)										{ ::btl::SMineBackCell cellToTest = board[coordToTest.y][coordToTest.x]; if(false == cellToTest.Mine && cellToTest.Hide) gpk_necall(::uncoverCell(gameState, coordToTest), "%s", "Out of memory?"); }
		coordToTest	= {cell.x	 , cell.y - 1	};	if(coordToTest.y >= 0)																{ ::btl::SMineBackCell cellToTest = board[coordToTest.y][coordToTest.x]; if(false == cellToTest.Mine && cellToTest.Hide) gpk_necall(::uncoverCell(gameState, coordToTest), "%s", "Out of memory?"); }
		coordToTest	= {cell.x + 1, cell.y - 1	};	if(coordToTest.y >= 0 && coordToTest.x < (int32_t)gridMetrix.x)						{ ::btl::SMineBackCell cellToTest = board[coordToTest.y][coordToTest.x]; if(false == cellToTest.Mine && cellToTest.Hide) gpk_necall(::uncoverCell(gameState, coordToTest), "%s", "Out of memory?"); }
		coordToTest	= {cell.x - 1, cell.y		};	if(coordToTest.x >= 0)																{ ::btl::SMineBackCell cellToTest = board[coordToTest.y][coordToTest.x]; if(false == cellToTest.Mine && cellToTest.Hide) gpk_necall(::uncoverCell(gameState, coordToTest), "%s", "Out of memory?"); }
		coordToTest	= {cell.x + 1, cell.y		};	if(coordToTest.x < (int32_t)gridMetrix.x)											{ ::btl::SMineBackCell cellToTest = board[coordToTest.y][coordToTest.x]; if(false == cellToTest.Mine && cellToTest.Hide) gpk_necall(::uncoverCell(gameState, coordToTest), "%s", "Out of memory?"); }
		coordToTest	= {cell.x - 1, cell.y + 1	};	if(coordToTest.y < (int32_t)gridMetrix.y && coordToTest.x >= 0)						{ ::btl::SMineBackCell cellToTest = board[coordToTest.y][coordToTest.x]; if(false == cellToTest.Mine && cellToTest.Hide) gpk_necall(::uncoverCell(gameState, coordToTest), "%s", "Out of memory?"); }
		coordToTest	= {cell.x	 , cell.y + 1	};	if(coordToTest.y < (int32_t)gridMetrix.y)											{ ::btl::SMineBackCell cellToTest = board[coordToTest.y][coordToTest.x]; if(false == cellToTest.Mine && cellToTest.Hide) gpk_necall(::uncoverCell(gameState, coordToTest), "%s", "Out of memory?"); }
		coordToTest	= {cell.x + 1, cell.y + 1	};	if(coordToTest.y < (int32_t)gridMetrix.y && coordToTest.x < (int32_t)gridMetrix.x)	{ ::btl::SMineBackCell cellToTest = board[coordToTest.y][coordToTest.x]; if(false == cellToTest.Mine && cellToTest.Hide) gpk_necall(::uncoverCell(gameState, coordToTest), "%s", "Out of memory?"); }
	}
	return 0;
}

::gpk::error_t						btl::SMineBack::Flag			(const ::gpk::SCoord2<uint32_t> cell)	{ ::btl::SMineBackCell & cellData = Board[cell.y][cell.x]; if(cellData.Hide) { cellData.Flag = !cellData.Flag; cellData.What = false; } return 0; }
::gpk::error_t						btl::SMineBack::Hold			(const ::gpk::SCoord2<uint32_t> cell)	{ ::btl::SMineBackCell & cellData = Board[cell.y][cell.x]; if(cellData.Hide) { cellData.What = !cellData.What; cellData.Flag = false; } return 0; }
::gpk::error_t						btl::SMineBack::Step			(const ::gpk::SCoord2<uint32_t> cell)	{ ::btl::SMineBackCell & cellData = Board[cell.y][cell.x];
	if(cellData.Hide) {
		if(cellData.Mine && false == cellData.Flag)
			cellData.Boom						= true;
		if(false == cellData.Flag)
			gpk_necall(::uncoverCell(*this, cell.template Cast<int32_t>()), "%s", "Out of memory?");
	}
	if(false == cellData.Boom) {
		::gpk::SImageMonochrome<uint64_t>		cellsMines; gpk_necall(cellsMines.resize(Board.metrics()), "%s", "Out of memory?");
		::gpk::SImageMonochrome<uint64_t>		cellsHides; gpk_necall(cellsHides.resize(Board.metrics()), "%s", "Out of memory?");
		const int32_t							totalHides						= GetHides(cellsHides.View);
		const int32_t							totalMines						= GetMines(cellsMines.View);
		if(totalHides <= totalMines && false == cellData.Boom)
			Time.Count							= ::gpk::timeCurrent() - Time.Offset;
	}
	return cellData.Boom ? 1 : 0;
}

static	::gpk::SCoord2<uint32_t>	getBlockCount				(const ::gpk::SCoord2<uint32_t> boardMetrics, const ::gpk::SCoord2<uint32_t> blockMetrics)	{
	return
		{ boardMetrics.x / blockMetrics.x + one_if(boardMetrics.x % blockMetrics.x)
		, boardMetrics.y / blockMetrics.y + one_if(boardMetrics.y % blockMetrics.y)
		};
}

static	::gpk::SCoord2<uint32_t>	getBlockFromCoord			(const ::gpk::SCoord2<uint32_t> boardMetrics, const ::gpk::SCoord2<uint32_t> blockMetrics)	{
	return
		{ boardMetrics.x / blockMetrics.x
		, boardMetrics.y / blockMetrics.y
		};
}

static	::gpk::SCoord2<uint32_t>	getLocalCoordFromCoord		(const ::gpk::SCoord2<uint32_t> boardMetrics, const ::gpk::SCoord2<uint32_t> blockMetrics)	{
	return
		{ boardMetrics.x % blockMetrics.x
		, boardMetrics.y % blockMetrics.y
		};
}

::gpk::error_t						btl::SMineBack::Start		(const ::gpk::SCoord2<uint32_t> boardMetrics, const uint32_t mineCount)	{
	Time.Offset							= ::gpk::timeCurrent();
	gpk_necall(Board.resize(boardMetrics, {1,}), "Out of memory? Board size: {%u, %u}", boardMetrics.x, boardMetrics.y);
	const ::gpk::SCoord2<uint32_t>			blockCount					= ::getBlockCount(boardMetrics, BLOCK_METRICS);
	gpk_necall(BoardBlocks.resize(blockCount.x * blockCount.y), "Out of memory? Board size: {%u, %u}", boardMetrics.x, boardMetrics.y);
	for(uint32_t iMine = 0; iMine < mineCount; ++iMine) {
		{ // Old small model (deprecated)
			::gpk::SCoord2<uint32_t>				cellPosition					= {rand() % boardMetrics.x, rand() % boardMetrics.y};
			::btl::SMineBackCell					* mineData						= &Board[cellPosition.y][cellPosition.x];
			while(mineData->Mine) {
				cellPosition						= {rand() % boardMetrics.x, rand() % boardMetrics.y};
				mineData							= &Board[cellPosition.y][cellPosition.x];
			}
			mineData->Mine						= true;
		}
		{ // Block-based model
			::gpk::SCoord2<uint32_t>				cellPosition					= {rand() % boardMetrics.x, rand() % boardMetrics.y};
			::gpk::SCoord2<uint32_t>				cellBlock						= getBlockFromCoord(cellPosition, BLOCK_METRICS);
			cellPosition						= getLocalCoordFromCoord(cellPosition, BLOCK_METRICS);
			uint32_t								blockIndex						= cellBlock.y * BLOCK_METRICS.x + cellBlock.x;
			BoardBlocks[blockIndex]->resize(BLOCK_METRICS);
			::btl::SMineBackCell					* mineData						= &(*BoardBlocks[blockIndex])[cellPosition.y][cellPosition.x];
			while(mineData->Mine) {
				cellPosition						= {rand() % boardMetrics.x, rand() % boardMetrics.y};
				cellBlock							= getBlockFromCoord(cellPosition, BLOCK_METRICS);
				cellPosition						= getLocalCoordFromCoord(cellPosition, BLOCK_METRICS);
				blockIndex							= cellBlock.y * BLOCK_METRICS.x + cellBlock.x;
				mineData							= &(*BoardBlocks[blockIndex])[cellPosition.y][cellPosition.x];
			}
			mineData->Mine						= true;
		}
	}
	return 0;
}


::gpk::error_t						btl::SMineBack::Save			(::gpk::array_pod<char_t> & bytes)	{
	::gpk::array_pod<byte_t>				rleDecoded						= {};
	gpk_necall(rleDecoded.append((const char*)&Board.metrics(), sizeof(::gpk::SCoord2<uint32_t>)) , "%s", "Out of memory?");
	gpk_necall(rleDecoded.append((const char*)Board.Texels.begin(), Board.Texels.size()), "%s", "Out of memory?");
	gpk_necall(::gpk::rleEncode(rleDecoded, bytes), "%s", "Out of memory?");
	gpk_necall(bytes.append((const char*)&Time, sizeof(::gpk::SRange<uint64_t>)), "%s", "Out of memory?");
	return 0;
}

::gpk::error_t						btl::SMineBack::Load			(::gpk::view_const_byte	bytes)	{
	Time								= *(::gpk::SRange<uint64_t>*)&bytes[bytes.size() - sizeof(::gpk::SRange<uint64_t>)];
	bytes								= {bytes.begin(), bytes.size() - sizeof(::gpk::SRange<uint64_t>)};
	::gpk::array_pod<byte_t>				gameStateBytes					= {};
	gpk_necall(::gpk::rleDecode(bytes, gameStateBytes), "%s", "Out of memory or corrupt data!");
	ree_if(gameStateBytes.size() < sizeof(::gpk::SCoord2<uint32_t>), "Invalid game state file format: %s.", "Invalid file size");
	::gpk::SCoord2<uint32_t>				boardMetrics					= *(::gpk::SCoord2<uint32_t>*)gameStateBytes.begin();
	gpk_necall(Board.resize(boardMetrics), "Out of memory? Board size: {%u, %u}", boardMetrics.x, boardMetrics.y);
	memcpy(Board.View.begin(), &gameStateBytes[sizeof(::gpk::SCoord2<uint32_t>)], Board.View.size());

	// --- The game is already loaded. Now, we need to get a significant return value from the data we just loaded.
	for(uint32_t y = 0; y < Board.metrics().y; ++y)
	for(uint32_t x = 0; x < Board.metrics().x; ++x)
		if(Board.View[y][x].Boom)
			return 1;
	::gpk::SImageMonochrome<uint64_t>		cellsMines; gpk_necall(cellsMines.resize(Board.metrics())	, "%s", "Out of memory?");
	::gpk::SImageMonochrome<uint64_t>		cellsHides; gpk_necall(cellsHides.resize(Board.metrics())	, "%s", "Out of memory?");
	const uint32_t							totalMines						= GetMines(cellsMines.View);
	const uint32_t							totalHides						= GetHides(cellsHides.View);
	::gpk::SImage<uint8_t>					hints;
	gpk_necall(hints.resize(Board.metrics(), 0), "%s", "");
	GetHints(hints.View);
	return (totalHides <= totalMines) ? 2 : 0;
}
