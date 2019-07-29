#include "btl_minesweeper.h"
#include "gpk_chrono.h"
#include "gpk_encoding.h"

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

::gpk::error_t						btl::SMineBack::GetBlast	(::gpk::SCoord2<uint32_t> & out_coord)	const {
	if(GameState.BlockBased)
		out_coord							= GameState.BlastCoord;
	else {
		const ::gpk::SCoord2<uint32_t>	gridMetrix = Board.metrics();
		for(uint32_t y = 0; y < gridMetrix.y; ++y) {
			for(uint32_t x = 0; x < gridMetrix.x; ++x) {
				const uint8_t							value						=	Board[y][x].Boom ? 1 : 0;
				if(value > 0) {
					out_coord							= {x, y};
					return 1;
				}
			}
		}
	}
	return 0;
}

#define ASSIGN_GRID_BOOL(value_, total_) {					\
		for(uint32_t y = 0; y < gridMetrix.y; ++y) {		\
			const uint32_t offset = y * gridMetrix.x;		\
			for(uint32_t x = 0; x < gridMetrix.x; ++x) {	\
				const ::btl::SMineBackCell * cellValue = 0;	\
				GetCell({x ,y}, &cellValue);				\
				if(0 == cellValue)							\
					continue;								\
				uint8_t	value =	cellValue->value_ ? 1 : 0;	\
				out_Cells[offset + x] = value;				\
				if(value)									\
					total_ += value;						\
			}												\
		}													\
	}

::gpk::error_t						btl::SMineBack::GetMines	(::gpk::view_bit<uint64_t> & out_Cells)	const	{ uint32_t total = 0;	const ::gpk::SCoord2<uint32_t> gridMetrix = (GameState.BlockBased) ? GameState.BoardSize : Board.metrics(); ASSIGN_GRID_BOOL(Mine, total); return total; }
::gpk::error_t						btl::SMineBack::GetFlags	(::gpk::view_bit<uint64_t> & out_Cells)	const	{ uint32_t total = 0;	const ::gpk::SCoord2<uint32_t> gridMetrix = (GameState.BlockBased) ? GameState.BoardSize : Board.metrics(); ASSIGN_GRID_BOOL(Flag, total); return total; }
::gpk::error_t						btl::SMineBack::GetHolds	(::gpk::view_bit<uint64_t> & out_Cells)	const	{ uint32_t total = 0;	const ::gpk::SCoord2<uint32_t> gridMetrix = (GameState.BlockBased) ? GameState.BoardSize : Board.metrics(); ASSIGN_GRID_BOOL(What, total); return total; }
::gpk::error_t						btl::SMineBack::GetHides	(::gpk::view_bit<uint64_t> & out_Cells)	const	{ uint32_t total = 0;	const ::gpk::SCoord2<uint32_t> gridMetrix = (GameState.BlockBased) ? GameState.BoardSize : Board.metrics(); ASSIGN_GRID_BOOL(Hide, total); return total; }
::gpk::error_t						btl::SMineBack::GetHints	(::gpk::view_grid<uint8_t> & out_Cells)	const	{						const ::gpk::SCoord2<uint32_t> gridMetrix = (GameState.BlockBased) ? GameState.BoardSize : Board.metrics();
	for(int32_t y = 0; y < (int32_t)gridMetrix.y; ++y)
	for(int32_t x = 0; x < (int32_t)gridMetrix.x; ++x) {
		const ::btl::SMineBackCell				* cellValue					= 0;
		GetCell({(uint32_t)x, (uint32_t)y}, &cellValue);
		if(0 == cellValue)
			continue;

		const uint8_t							hidden						= cellValue->Hide ? 1 : 0; (void)hidden;
		//if(cellValue.Hide)
		//	continue;
		if(cellValue->Mine)
			continue;
		uint8_t									& out				= out_Cells[y][x]		= 0;
		::gpk::SCoord2<int32_t>					coordToTest			= {};	// actually by making this uint32_t we could easily change all the conditions to be coordToTest.i < gridMetrix.i. However, I'm too lazy to start optimizing what's hardly the bottleneck
		coordToTest							= {x - 1, y - 1	};	if(coordToTest.y >= 0 && coordToTest.x >= 0)										{ const ::btl::SMineBackCell * cellValueToTest = 0; if(-1 != GetCell(coordToTest.Cast<uint32_t>(), &cellValueToTest)) if(cellValueToTest->Mine) ++out; }
		coordToTest							= {x	, y - 1	};	if(coordToTest.y >= 0)																{ const ::btl::SMineBackCell * cellValueToTest = 0; if(-1 != GetCell(coordToTest.Cast<uint32_t>(), &cellValueToTest)) if(cellValueToTest->Mine) ++out; }
		coordToTest							= {x + 1, y - 1	};	if(coordToTest.y >= 0 && coordToTest.x < (int32_t)gridMetrix.x)						{ const ::btl::SMineBackCell * cellValueToTest = 0; if(-1 != GetCell(coordToTest.Cast<uint32_t>(), &cellValueToTest)) if(cellValueToTest->Mine) ++out; }
		coordToTest							= {x - 1, y		};	if(coordToTest.x >= 0)																{ const ::btl::SMineBackCell * cellValueToTest = 0; if(-1 != GetCell(coordToTest.Cast<uint32_t>(), &cellValueToTest)) if(cellValueToTest->Mine) ++out; }
		coordToTest							= {x + 1, y		};	if(coordToTest.x < (int32_t)gridMetrix.x)											{ const ::btl::SMineBackCell * cellValueToTest = 0; if(-1 != GetCell(coordToTest.Cast<uint32_t>(), &cellValueToTest)) if(cellValueToTest->Mine) ++out; }
		coordToTest							= {x - 1, y + 1	};	if(coordToTest.y < (int32_t)gridMetrix.y && coordToTest.x >= 0)						{ const ::btl::SMineBackCell * cellValueToTest = 0; if(-1 != GetCell(coordToTest.Cast<uint32_t>(), &cellValueToTest)) if(cellValueToTest->Mine) ++out; }
		coordToTest							= {x	, y + 1	};	if(coordToTest.y < (int32_t)gridMetrix.y)											{ const ::btl::SMineBackCell * cellValueToTest = 0; if(-1 != GetCell(coordToTest.Cast<uint32_t>(), &cellValueToTest)) if(cellValueToTest->Mine) ++out; }
		coordToTest							= {x + 1, y + 1	};	if(coordToTest.y < (int32_t)gridMetrix.y && coordToTest.x < (int32_t)gridMetrix.x)	{ const ::btl::SMineBackCell * cellValueToTest = 0; if(-1 != GetCell(coordToTest.Cast<uint32_t>(), &cellValueToTest)) if(cellValueToTest->Mine) ++out; }
	}
	return 0;
}

static	::gpk::error_t				uncoverCell						(::btl::SMineBack & gameState, const ::gpk::SCoord2<int32_t> cell) {
	::btl::SMineBackCell					* currentCell					= 0;
	gameState.GetCell(cell.Cast<uint32_t>(), &currentCell);
	currentCell->Hide					= false;
	const ::gpk::SCoord2<uint32_t>			gridMetrix						= gameState.GameState.BlockBased ? gameState.GameState.BoardSize : gameState.Board.metrics();
	if(false == gameState.GameState.BlockBased) {
		::gpk::view_grid<::btl::SMineBackCell>	board							= gameState.Board.View;
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
	}
	return 0;
}

::gpk::error_t						btl::SMineBack::Flag			(const ::gpk::SCoord2<uint32_t> cellCoord)	{ ::btl::SMineBackCell * cellData = 0; GetCell(cellCoord, &cellData); if(cellData->Hide) { cellData->Flag = !cellData->Flag; cellData->What = false; } return 0; }
::gpk::error_t						btl::SMineBack::Hold			(const ::gpk::SCoord2<uint32_t> cellCoord)	{ ::btl::SMineBackCell * cellData = 0; GetCell(cellCoord, &cellData); if(cellData->Hide) { cellData->What = !cellData->What; cellData->Flag = false; } return 0; }
::gpk::error_t						btl::SMineBack::Step			(const ::gpk::SCoord2<uint32_t> cellCoord)	{ ::btl::SMineBackCell * cellData = 0; GetCell(cellCoord, &cellData);
	if(cellData->Hide) {
		if(cellData->Mine && false == cellData->Flag)
			cellData->Boom						= true;
		else if(false == cellData->Flag)
			gpk_necall(::uncoverCell(*this, cellCoord.Cast<int32_t>()), "%s", "Out of memory?");

		if(false == cellData->Boom) {	// Check if we won after uncovering the cell(s)
			::gpk::SImageMonochrome<uint64_t>		cellsMines;
			::gpk::SImageMonochrome<uint64_t>		cellsHides;
			::gpk::SCoord2							boardMetrics				= GameState.BlockBased ? GameState.BoardSize : Board.metrics();
			gpk_necall(cellsMines.resize(boardMetrics), "%s", "Out of memory?");
			gpk_necall(cellsHides.resize(boardMetrics), "%s", "Out of memory?");
			const int32_t							totalHides						= GetHides(cellsHides.View);
			const int32_t							totalMines						= GetMines(cellsMines.View);
			if(totalHides <= totalMines && false == cellData->Boom)	// Win!
				GameState.Time.Count				= ::gpk::timeCurrent() - GameState.Time.Offset;
		}
		else {
			GameState.BlastCoord				= cellCoord;
			GameState.Blast						= true;
		}
	}
	return cellData->Boom ? 1 : 0;
}

::gpk::error_t						btl::SMineBack::Start		(const ::gpk::SCoord2<uint32_t> boardMetrics, const uint32_t mineCount)	{
	GameState							= {};
	GameState.BoardSize					= boardMetrics;
	GameState.MineCount					= mineCount;
	GameState.Time.Offset				= ::gpk::timeCurrent();
	if(false == GameState.BlockBased) { // Old small model (deprecated)
		gpk_necall(Board.resize(boardMetrics, {1,}), "Out of memory? Board size: {%u, %u}", boardMetrics.x, boardMetrics.y);
		for(uint32_t iMine = 0; iMine < mineCount; ++iMine) {
			::gpk::SCoord2<uint32_t>				cellPosition					= {rand() % boardMetrics.x, rand() % boardMetrics.y};
			::btl::SMineBackCell					* mineData						= &Board[cellPosition.y][cellPosition.x];
			while(mineData->Mine) {
				cellPosition						= {rand() % boardMetrics.x, rand() % boardMetrics.y};
				mineData							= &Board[cellPosition.y][cellPosition.x];
			}
			mineData->Mine						= true;
		}
	}
	else { // Block-based model
		const ::gpk::SCoord2<uint32_t>			blockCount					= ::getBlockCount(boardMetrics, GameState.BlockSize);
		gpk_necall(BoardBlocks.resize(blockCount.x * blockCount.y), "Out of memory? Board size: {%u, %u}", boardMetrics.x, boardMetrics.y);
		for(uint32_t iMine = 0; iMine < mineCount; ++iMine) {
			::gpk::SCoord2<uint32_t>				cellPosition					= {rand() % boardMetrics.x, rand() % boardMetrics.y};
			::btl::SMineBackCell					* cellData						= 0;
			GetCell(cellPosition, &cellData);
			while(cellData->Mine)
				GetCell(cellPosition, &cellData);
			cellData->Mine						= true;
		}
	}
	return 0;
}

::gpk::error_t						btl::SMineBack::Save			(::gpk::array_pod<char_t> & bytes)	{
	if(false == GameState.BlockBased) {
		::gpk::array_pod<byte_t>				rleDecoded						= {};
		gpk_necall(rleDecoded.append((const char*)&Board.metrics(), sizeof(::gpk::SCoord2<uint32_t>)) , "%s", "Out of memory?");
		gpk_necall(rleDecoded.append((const char*)Board.Texels.begin(), Board.Texels.size()), "%s", "Out of memory?");
		gpk_necall(::gpk::rleEncode(rleDecoded, bytes), "%s", "Out of memory?");
		gpk_necall(bytes.append((const char*)&GameState.Time, sizeof(::gpk::SRange<uint64_t>)), "%s", "Out of memory?");
	}
	else {
		::gpk::array_pod<byte_t>				rleDecoded						= {};
		gpk_necall(rleDecoded.append((const char*)&BoardBlocks.size(), sizeof(uint32_t)) , "%s", "Out of memory?");
		for(uint32_t iBlock = 0; iBlock < BoardBlocks.size(); ++iBlock) {
			gpk_necall(rleDecoded.append((const char*)&BoardBlocks[iBlock]->metrics(), sizeof(::gpk::SCoord2<uint32_t>)) , "%s", "Out of memory?");
			gpk_necall(rleDecoded.append((const char*)BoardBlocks[iBlock]->Texels.begin(), BoardBlocks[iBlock]->Texels.size()), "%s", "Out of memory?");
		}
		gpk_necall(::gpk::rleEncode(rleDecoded, bytes), "%s", "Out of memory?");
		gpk_necall(bytes.append((const char*)&GameState, sizeof(::btl::SMineBackState)), "%s", "Out of memory?");
	}
	return 0;
}

::gpk::error_t						btl::SMineBack::Load			(::gpk::view_const_byte	bytes)	{
	if(false == GameState.BlockBased) {
		GameState.Time						= *(::gpk::SRange<uint64_t>*)&bytes[bytes.size() - sizeof(::gpk::SRange<uint64_t>)];
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
	else {
		GameState							= *(::btl::SMineBackState*)&bytes[bytes.size() - sizeof(::btl::SMineBackState)];
		bytes								= {bytes.begin(), bytes.size() - sizeof(::btl::SMineBackState)};
		::gpk::array_pod<byte_t>				gameStateBytes					= {};
		gpk_necall(::gpk::rleDecode(bytes, gameStateBytes), "%s", "Out of memory or corrupt data!");
		ree_if(gameStateBytes.size() < sizeof(uint32_t), "Invalid game state file format: %s.", "Invalid file size");
		uint32_t								blockCount						= *(uint32_t*)gameStateBytes.begin();
		gpk_necall(BoardBlocks.resize(blockCount), "%s", "Corrupt file?");
		uint32_t								iByteOffset						= sizeof(uint32_t);
		for(uint32_t iBlock = 0; iBlock < blockCount; ++iBlock) {
			GameState.BlockSize					= *(::gpk::SCoord2<uint32_t>*)gameStateBytes[iByteOffset];
			iByteOffset							+= sizeof(::gpk::SCoord2<uint32_t>);
			gpk_necall(BoardBlocks[iBlock]->resize(GameState.BlockSize), "Out of memory? Board size: {%u, %u}", GameState.BlockSize.x, GameState.BlockSize.y);
			memcpy(BoardBlocks[iBlock]->View.begin(), &gameStateBytes[iByteOffset + sizeof(::gpk::SCoord2<uint32_t>)], BoardBlocks[iBlock]->View.size());
			iByteOffset							+= BoardBlocks[iBlock]->View.size();
		}
		if(GameState.Blast)
			return 1;
		if(GameState.Time.Count)
			return 2;
		return 0;
	}
}

::gpk::error_t									btl::SMineBack::GetCell				(const ::gpk::SCoord2<uint32_t> & cellCoord, ::btl::SMineBackCell ** out_cell)			{
	if(cellCoord.x > GameState.BoardSize.x || cellCoord.x > GameState.BoardSize.y)
		return -1;
	if(false == GameState.BlockBased)
		*out_cell										= &Board[cellCoord.y][cellCoord.x];
	else {
		::gpk::SCoord2<uint32_t>				cellPosition					= {rand() % GameState.BoardSize.x, rand() % GameState.BoardSize.y};
		::gpk::SCoord2<uint32_t>				cellBlock						= getBlockFromCoord(cellPosition, GameState.BlockSize);
		cellPosition						= getLocalCoordFromCoord(cellPosition, GameState.BlockSize);
		uint32_t								blockIndex						= cellBlock.y * GameState.BlockSize.x + cellBlock.x;
		if(0 == BoardBlocks[blockIndex])
			BoardBlocks[blockIndex]->resize(GameState.BlockSize, {true, });
		*out_cell							= &(*BoardBlocks[blockIndex])[cellPosition.y][cellPosition.x];
	}
	return 0;
}

::gpk::error_t									btl::SMineBack::GetCell				(const ::gpk::SCoord2<uint32_t> & cellCoord, const ::btl::SMineBackCell ** out_cell)		const	{
	if(cellCoord.x > GameState.BoardSize.x || cellCoord.x > GameState.BoardSize.y)
		return -1;
	if(false == GameState.BlockBased)
		*out_cell										= &Board[cellCoord.y][cellCoord.x];
	else {
		::gpk::SCoord2<uint32_t>				cellPosition					= {rand() % GameState.BoardSize.x, rand() % GameState.BoardSize.y};
		::gpk::SCoord2<uint32_t>				cellBlock						= getBlockFromCoord(cellPosition, GameState.BlockSize);
		cellPosition						= getLocalCoordFromCoord(cellPosition, GameState.BlockSize);
		uint32_t								blockIndex						= cellBlock.y * GameState.BlockSize.x + cellBlock.x;
		*out_cell							= (0 == BoardBlocks[blockIndex]) ? 0 : &(*BoardBlocks[blockIndex])[cellPosition.y][cellPosition.x];
	}
	return 0;
}

