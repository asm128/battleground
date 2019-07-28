#include "gpk_image.h"

#ifndef BTL_MINESWEEPER_H_20398423
#define BTL_MINESWEEPER_H_20398423

namespace btl
{
#pragma pack(push, 1)	// 0.5 would be much better, but I suppose this will do as it isn't even necessary for our small struct
	// Holds the state for a single cell
	struct SMineBackCell {
		bool								Hide			: 1;
		bool								Mine			: 1;
		bool								Flag			: 1;
		bool								What			: 1;
		bool								Boom			: 1;
	};
#pragma pack(pop)

	// Holds the board state
	struct SMineBack {
		::gpk::SRange<uint64_t>				Time			= {};
		::gpk::SImage<SMineBackCell>		Board;

		::gpk::error_t						GetMines		(::gpk::view_bit<uint64_t> & out_Cells)	const;	// These functions return the amount of mines in the board.
		::gpk::error_t						GetFlags		(::gpk::view_bit<uint64_t> & out_Cells)	const;	// These functions return the amount of flags in the board.
		::gpk::error_t						GetWhats		(::gpk::view_bit<uint64_t> & out_Cells)	const;	// These functions return the amount of whats in the board.
		::gpk::error_t						GetHides		(::gpk::view_bit<uint64_t> & out_Cells)	const;	// These functions return the amount of hides in the board.
		::gpk::error_t						GetSafes		(::gpk::view_bit<uint64_t> & out_Cells)	const;	// These functions return the amount of safes in the board.
		::gpk::error_t						GetHints		(::gpk::view_grid<uint8_t> & out_Cells)	const;	// These functions return the amount of hints in the board.
		::gpk::error_t						GetBlast		(::gpk::SCoord2<uint32_t> & out_coord)	const;	// Returns 1 if blast was found, 0 if not.

		::gpk::error_t						Flag			(const ::gpk::SCoord2<uint32_t> cell);	// Set/Clear a flag on a given tile
		::gpk::error_t						Hold			(const ::gpk::SCoord2<uint32_t> cell);	// Set/Clear a question mark on a given tile
		::gpk::error_t						Step			(const ::gpk::SCoord2<uint32_t> cell);	// Step on a given tile
		::gpk::error_t						Start			(const ::gpk::SCoord2<uint32_t> boardMetrics, const uint32_t mineCount);	// Start a new game with a board of boardMetrics size and mineCount mine count.

		::gpk::error_t						Save			(::gpk::array_pod<char_t> & bytes);	// Write game state to array
		::gpk::error_t						Load			(::gpk::view_const_byte		bytes);	// Read game state from view


	};
} // namespace

#endif // BTL_MINESWEEPER_H_20398423
