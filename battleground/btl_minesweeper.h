#include "gpk_image.h"

#ifndef BTL_MINESWEEPER_H_20398423
#define BTL_MINESWEEPER_H_20398423

namespace btl
{
#pragma pack(push, 1)	// 0.5 would be much better, but I suppose this will do as it isn't even necessary for our small struct
	// Holds the state for a single cell
	struct SMineSweeperCell {
		bool								Hide			: 1;
		bool								Mine			: 1;
		bool								Flag			: 1;
		bool								What			: 1;
		bool								Boom			: 1;
	};
#pragma pack(pop)

	// Holds the board state
	struct SMineSweeper {
		::gpk::SImage<SMineSweeperCell>		Board;

		::gpk::error_t						GetMines		(::gpk::view_bit<uint64_t> & out_Cells)	const;
		::gpk::error_t						GetFlags		(::gpk::view_bit<uint64_t> & out_Cells)	const;
		::gpk::error_t						GetWhats		(::gpk::view_bit<uint64_t> & out_Cells)	const;
		::gpk::error_t						GetHides		(::gpk::view_bit<uint64_t> & out_Cells)	const;
		::gpk::error_t						GetSafes		(::gpk::view_bit<uint64_t> & out_Cells)	const;
		::gpk::error_t						GetHints		(::gpk::view_grid<uint8_t> & out_Cells)	const;
		::gpk::error_t						GetBlast		(::gpk::SCoord2<uint32_t> & out_coord)	const;

		::gpk::error_t						Flag			(const ::gpk::SCoord2<uint32_t> cell);
		::gpk::error_t						Hold			(const ::gpk::SCoord2<uint32_t> cell);
		::gpk::error_t						Step			(const ::gpk::SCoord2<uint32_t> cell);
		::gpk::error_t						Start			(const ::gpk::SCoord2<uint32_t> boardMetrics, const uint32_t mineCount);
	};
} // namespace

#endif // BTL_MINESWEEPER_H_20398423
