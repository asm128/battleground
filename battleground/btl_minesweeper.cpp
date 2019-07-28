#include "btl_minesweeper.h"
#include "gpk_chrono.h"
#include "gpk_encoding.h"

::gpk::error_t				btl::SMineBack::GetBlast	(::gpk::SCoord2<uint32_t> & out_coord)	const {
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

::gpk::error_t				btl::SMineBack::GetMines	(::gpk::view_bit<uint64_t> & out_Cells)	const { const ::gpk::SCoord2<uint32_t> gridMetrix = Board.metrics(); uint32_t total = 0; ASSIGN_GRID_BOOL(Board[y][x].Mine, total); return total; }
::gpk::error_t				btl::SMineBack::GetFlags	(::gpk::view_bit<uint64_t> & out_Cells)	const { const ::gpk::SCoord2<uint32_t> gridMetrix = Board.metrics(); uint32_t total = 0; ASSIGN_GRID_BOOL(Board[y][x].Flag, total); return total; }
::gpk::error_t				btl::SMineBack::GetWhats	(::gpk::view_bit<uint64_t> & out_Cells)	const { const ::gpk::SCoord2<uint32_t> gridMetrix = Board.metrics(); uint32_t total = 0; ASSIGN_GRID_BOOL(Board[y][x].What, total); return total; }
::gpk::error_t				btl::SMineBack::GetHides	(::gpk::view_bit<uint64_t> & out_Cells)	const { const ::gpk::SCoord2<uint32_t> gridMetrix = Board.metrics(); uint32_t total = 0; ASSIGN_GRID_BOOL(Board[y][x].Hide, total); return total; }
::gpk::error_t				btl::SMineBack::GetSafes	(::gpk::view_bit<uint64_t> & out_Cells)	const { const ::gpk::SCoord2<uint32_t> gridMetrix = Board.metrics(); uint32_t total = 0;
	::gpk::SImage<uint8_t>			hints;
	hints.resize(Board.metrics());
	this->GetHints(hints.View);
	ASSIGN_GRID_BOOL(0 == hints[y][x] && false == Board[y][x].Mine, total);
	//ASSIGN_GRID_BOOL(false == Board[y][x].Hide && 0 == hints[y][x] && false == Board[y][x].Mine, total);
	return total;
}

::gpk::error_t				btl::SMineBack::GetHints	(::gpk::view_grid<uint8_t> & out_Cells)	const { const ::gpk::SCoord2<uint32_t> gridMetrix = Board.metrics(); uint32_t total = 0;
	for(int32_t y = 0; y < (int32_t)gridMetrix.y; ++y) {
		for(int32_t x = 0; x < (int32_t)gridMetrix.x; ++x) {
			const ::btl::SMineBackCell		cellValue			= Board[y][x];
			const uint8_t					hidden				= cellValue.Hide ? 1 : 0; (void)hidden;
			//if(cellValue.Hide)
			//	continue;
			if(cellValue.Mine)
				continue;
			uint8_t							& out				= out_Cells[y][x]		= 0;
			::gpk::SCoord2<int32_t>			coordToTest			= {};
			coordToTest					= {x - 1, y - 1	};	if(coordToTest.y >= 0 && coordToTest.x >= 0)											{ if(Board[coordToTest.y][coordToTest.x].Mine) ++out; }
			coordToTest					= {x	, y - 1	};	if(coordToTest.y >= 0)																	{ if(Board[coordToTest.y][coordToTest.x].Mine) ++out; }
			coordToTest					= {x + 1, y - 1	};	if(coordToTest.y >= 0 && coordToTest.x < (int32_t)gridMetrix.x)							{ if(Board[coordToTest.y][coordToTest.x].Mine) ++out; }

			coordToTest					= {x - 1, y		};	if(coordToTest.x >= 0)																	{ if(Board[coordToTest.y][coordToTest.x].Mine) ++out; }
			coordToTest					= {x + 1, y		};	if(coordToTest.x < (int32_t)gridMetrix.x)												{ if(Board[coordToTest.y][coordToTest.x].Mine) ++out; }

			coordToTest					= {x - 1, y + 1	};	if(coordToTest.y < (int32_t)gridMetrix.y && coordToTest.x >= 0)							{ if(Board[coordToTest.y][coordToTest.x].Mine) ++out; }
			coordToTest					= {x	, y + 1	};	if(coordToTest.y < (int32_t)gridMetrix.y)												{ if(Board[coordToTest.y][coordToTest.x].Mine) ++out; }
			coordToTest					= {x + 1, y + 1	};	if(coordToTest.y < (int32_t)gridMetrix.y && coordToTest.x < (int32_t)gridMetrix.x)		{ if(Board[coordToTest.y][coordToTest.x].Mine) ++out; }
		}
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
		::gpk::SCoord2<int32_t>					coordToTest						= {};
		coordToTest	= {cell.x - 1, cell.y - 1	};	if(coordToTest.y >= 0 && coordToTest.x >= 0)										{ if(false == board[coordToTest.y][coordToTest.x].Mine && board[coordToTest.y][coordToTest.x].Hide) ::uncoverCell(gameState, coordToTest); }
		coordToTest	= {cell.x	 , cell.y - 1	};	if(coordToTest.y >= 0)																{ if(false == board[coordToTest.y][coordToTest.x].Mine && board[coordToTest.y][coordToTest.x].Hide) ::uncoverCell(gameState, coordToTest); }
		coordToTest	= {cell.x + 1, cell.y - 1	};	if(coordToTest.y >= 0 && coordToTest.x < (int32_t)gridMetrix.x)						{ if(false == board[coordToTest.y][coordToTest.x].Mine && board[coordToTest.y][coordToTest.x].Hide) ::uncoverCell(gameState, coordToTest); }
		coordToTest	= {cell.x - 1, cell.y		};	if(coordToTest.x >= 0)																{ if(false == board[coordToTest.y][coordToTest.x].Mine && board[coordToTest.y][coordToTest.x].Hide) ::uncoverCell(gameState, coordToTest); }
		coordToTest	= {cell.x + 1, cell.y		};	if(coordToTest.x < (int32_t)gridMetrix.x)											{ if(false == board[coordToTest.y][coordToTest.x].Mine && board[coordToTest.y][coordToTest.x].Hide) ::uncoverCell(gameState, coordToTest); }
		coordToTest	= {cell.x - 1, cell.y + 1	};	if(coordToTest.y < (int32_t)gridMetrix.y && coordToTest.x >= 0)						{ if(false == board[coordToTest.y][coordToTest.x].Mine && board[coordToTest.y][coordToTest.x].Hide) ::uncoverCell(gameState, coordToTest); }
		coordToTest	= {cell.x	 , cell.y + 1	};	if(coordToTest.y < (int32_t)gridMetrix.y)											{ if(false == board[coordToTest.y][coordToTest.x].Mine && board[coordToTest.y][coordToTest.x].Hide) ::uncoverCell(gameState, coordToTest); }
		coordToTest	= {cell.x + 1, cell.y + 1	};	if(coordToTest.y < (int32_t)gridMetrix.y && coordToTest.x < (int32_t)gridMetrix.x)	{ if(false == board[coordToTest.y][coordToTest.x].Mine && board[coordToTest.y][coordToTest.x].Hide) ::uncoverCell(gameState, coordToTest); }
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
			::uncoverCell(*this, cell.template Cast<int32_t>());
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

::gpk::error_t						btl::SMineBack::Start		(const ::gpk::SCoord2<uint32_t> boardMetrics, const uint32_t mineCount)	{
	Time.Offset							= ::gpk::timeCurrent();
	gpk_necall(Board.resize(boardMetrics, {1,}), "Out of memory? Board size: {%u, %u}", boardMetrics.x, boardMetrics.y);
	for(uint32_t iMine = 0; iMine < mineCount; ++iMine) {
		::gpk::SCoord2<uint32_t>							cellPosition					= {rand() % boardMetrics.x, rand() % boardMetrics.y};
		::btl::SMineBackCell								* mineData						= &Board[cellPosition.y][cellPosition.x];
		while(mineData->Mine) {
			cellPosition					= {rand() % boardMetrics.x, rand() % boardMetrics.y};
			mineData						= &Board[cellPosition.y][cellPosition.x];
		}
		mineData->Mine									= true;
	}
	return 0;
}


::gpk::error_t						btl::SMineBack::Save			(::gpk::array_pod<char_t> & bytes)	{
	::gpk::array_pod<byte_t>				rleDecoded						= {};
	gpk_necall(rleDecoded.append((const char*)&Board.metrics(), sizeof(::gpk::SCoord2<uint32_t>)) , "%s", "Out of memory?");
	gpk_necall(rleDecoded.append((const char*)Board.Texels.begin(), Board.Texels.size()), "%s", "Out of memory?");
	::gpk::rleEncode(rleDecoded, bytes);
	bytes.append((const char*)&Time, sizeof(::gpk::SRange<uint64_t>));
	return 0;
}

::gpk::error_t						btl::SMineBack::Load			(::gpk::view_const_byte	bytes)	{
	Time								= *(::gpk::SRange<uint64_t>*)&bytes[bytes.size() - sizeof(::gpk::SRange<uint64_t>)];
	bytes								= {bytes.begin(), bytes.size() - sizeof(::gpk::SRange<uint64_t>)};
	::gpk::array_pod<byte_t>				gameStateBytes					= {};
	::gpk::rleDecode(bytes, gameStateBytes);
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
