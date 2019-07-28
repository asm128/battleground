#include "btl_minesweeper.h"

#include "gpk_cgi_app_impl_v2.h"

#include "gpk_find.h"
#include "gpk_storage.h"
#include "gpk_process.h"
#include "gpk_json_expression.h"
#include "gpk_stdstring.h"
#include "gpk_chrono.h"
#include "gpk_parse.h"
#include "gpk_base64.h"
#include "gpk_encoding.h"

GPK_CGI_JSON_APP_IMPL();

GDEFINE_ENUM_TYPE	(MYSWEEPER_CMD, uint8_t		);
GDEFINE_ENUM_VALUE	(MYSWEEPER_CMD, Start		, 0);
GDEFINE_ENUM_VALUE	(MYSWEEPER_CMD, Continue	, 1);
GDEFINE_ENUM_VALUE	(MYSWEEPER_CMD, Step		, 2);
GDEFINE_ENUM_VALUE	(MYSWEEPER_CMD, Flag		, 3);
GDEFINE_ENUM_VALUE	(MYSWEEPER_CMD, Wipe		, 4);
GDEFINE_ENUM_VALUE	(MYSWEEPER_CMD, What		, 5);

::gpk::error_t									gameStateId						(::gpk::array_pod<char_t> & fileName, const ::gpk::view_const_string & ip, const ::gpk::view_const_string & port)	{
	::gpk::array_pod<char_t>							temp;
	gpk_necall(temp.append(ip)		, "%s", "Out of memory?");
	gpk_necall(temp.push_back('_')	, "%s", "Out of memory?");
	gpk_necall(temp.append(port)	, "%s", "Out of memory?");
	char												nowstr[128]						= {};
	uint64_t											now								= ::gpk::timeCurrentInUs();
	sprintf_s(nowstr, "_%.llu", now);
	gpk_necall(temp.append(::gpk::view_const_string{nowstr})	, "%s", "Out of memory?");
	::gpk::base64EncodeFS(temp, fileName);
	return 0;
}

::gpk::error_t									gameStateSave					(::btl::SMineSweeper & gameState, const ::gpk::view_const_string & idGame)	{
	::gpk::array_pod<byte_t>							gameStateBytes					= {};
	gpk_necall(gameStateBytes.append((const char*)&gameState.Board.metrics(), sizeof(::gpk::SCoord2<uint32_t>)) , "%s", "Out of memory?");
	gpk_necall(gameStateBytes.append((const char*)gameState.Board.Texels.begin(), gameState.Board.Texels.size()), "%s", "Out of memory?");
	::gpk::array_pod<byte_t>							rleEncoded						= {};
	::gpk::rleEncode(gameStateBytes, rleEncoded);
	::gpk::array_pod<char_t>							fileName						= idGame;
	fileName.append(".bms");
	gpk_necall(::gpk::fileFromMemory({fileName.begin(), fileName.size()}, rleEncoded), "Failed to write to file '%s'.", fileName.begin());
	return 0;
}

::gpk::error_t									gameStateLoad					(::btl::SMineSweeper & gameState, const ::gpk::view_const_string & idGame)	{
	::gpk::array_pod<char_t>							fileName						= idGame;
	fileName.append(".bms");
	::gpk::array_pod<byte_t>							rleEncoded						= {};
	gpk_necall(::gpk::fileToMemory({fileName.begin(), fileName.size()}, rleEncoded), "Failed to load game state file '%s'.", fileName.begin());
	::gpk::array_pod<byte_t>							gameStateBytes					= {};
	::gpk::rleDecode(rleEncoded, gameStateBytes);
	ree_if(gameStateBytes.size() < sizeof(::gpk::SCoord2<uint32_t>), "Invalid game state file format: %s.", "Invalid file size");
	::gpk::SCoord2<uint32_t>							boardMetrics					= *(::gpk::SCoord2<uint32_t>*)gameStateBytes.begin();
	gpk_necall(gameState.Board.resize(boardMetrics), "Out of memory? Board size: {%u, %u}", boardMetrics.x, boardMetrics.y);
	memcpy(gameState.Board.View.begin(), &gameStateBytes[sizeof(::gpk::SCoord2<uint32_t>)], gameState.Board.View.size());
	for(uint32_t y = 0; y < gameState.Board.metrics().y; ++y)
	for(uint32_t x = 0; x < gameState.Board.metrics().x; ++x)
		if(gameState.Board.View[y][x].Boom)
			return 1;
	::gpk::SImageMonochrome<uint64_t>					cellsMines; gpk_necall(cellsMines.resize(gameState.Board.metrics())	, "%s", "Out of memory?");
	::gpk::SImageMonochrome<uint64_t>					cellsHides; gpk_necall(cellsHides.resize(gameState.Board.metrics())	, "%s", "Out of memory?");
	const uint32_t										totalMines		= gameState.GetMines(cellsMines.View);
	const uint32_t										totalHides		= gameState.GetHides(cellsHides.View);
	::gpk::SImage<uint8_t>								hints;
	gpk_necall(hints.resize(gameState.Board.metrics(), 0), "%s", "");
	gameState.GetHints(hints.View);
	return (totalHides <= totalMines) ? 2 : 0;
}

::gpk::error_t									Continue						(::btl::SMineSweeper & gameState)										{ (void)gameState; return 0; }

static	const ::gpk::view_const_string			STR_RESPONSE_METHOD_INVALID		= "{ \"status\" : 403, \"description\" :\"Forbidden - Available method/command combinations are: \n- GET / Start, \n- POST / Continue, \n- POST / Step, \n- POST / Flag, \n- POST / Wipe, \n- POST / Hold.\" }\r\n";

::gpk::error_t									output_board_generate			(const ::gpk::SCoord2<uint32_t> & boardMetrics, const ::gpk::view_bit<uint64_t> & cells, ::gpk::array_pod<char_t> & output)	{
	gpk_necall(output.push_back('[')									, "%s", "Out of memory?");
	for(uint32_t row = 0; row < boardMetrics.y; ++row) {
		gpk_necall(output.push_back('\n'), "%s", "Out of memory?");
		gpk_necall(output.push_back('['), "%s", "Out of memory?");
		for(uint32_t x = 0; x < boardMetrics.x; ++x) {
			const bool									cell							= cells[row * boardMetrics.x + x];
			gpk_necall(output.push_back(cell ? '1' : '0'), "%s", "Out of memory?");
			if(x < (boardMetrics.x - 1))
				gpk_necall(output.push_back(','), "%s", "Out of memory?");
		}
		gpk_necall(output.push_back(']'), "%s", "Out of memory?");
		if(row < (boardMetrics.y - 1))
			gpk_necall(output.push_back(','), "%s", "Out of memory?");
	}
	gpk_necall(output.push_back('\n'), "%s", "Out of memory?");
	gpk_necall(output.push_back(']')									, "%s", "Out of memory?");
	return 0;
}

::gpk::error_t									output_error_invalid_cell		(const ::gpk::SCoord2<uint32_t> & actionCellCoord, ::gpk::array_pod<char_t> & output)	{
	char												temp			[256]			= {};
	static constexpr	const char						responseFormat	[]				= "{ \"status\" : 400, \"description\" :\"Bad Request - Invalid cell: '{\"x\": %u, \"u\": %u}'.\" }\r\n";
	sprintf_s(temp, responseFormat, actionCellCoord.x, actionCellCoord.y);
	gpk_necall(output.append(::gpk::view_const_string{temp}), "%s", temp);
	return 0;
}

::gpk::error_t									gpk_cgi_generate_output			(::gpk::SCGIRuntimeValues & runtimeValues, ::gpk::array_pod<char_t> & output)	{
	::srand((uint32_t)::gpk::timeCurrentInUs());

	::gpk::SHTTPAPIRequest								requestReceived					= {};
	bool												isCGIEnviron					= ::gpk::httpRequestInit(requestReceived, runtimeValues, true);
	::gpk::array_obj<::gpk::TKeyValConstString>			environViews;
	if (isCGIEnviron) {
		gpk_necall(output.append(::gpk::view_const_string{"Content-type: application/json\r\n"}), "%s", "Out of memory?");
		gpk_necall(output.append(::gpk::view_const_string{"\r\n"})								, "%s", "Out of memory?");
		const ::gpk::view_const_string						allowedMethods	[]				= {"GET", "POST"};
		gpk_necall(::gpk::environmentBlockViews(runtimeValues.EntryPointArgs.EnvironmentBlock, environViews), "%s", "If this breaks, we better know ASAP.");
		if(0 == ::gpk::keyValVerify(environViews, "REQUEST_METHOD", allowedMethods)) {
			gpk_necall(output.append(STR_RESPONSE_METHOD_INVALID), "%s", "Out of memory?");
			return 1;
		}
	}

	::gpk::array_pod<char_t>							strIdGame;
	::gpk::view_const_string							idGame;
	::btl::SMineSweeper									gameState						= {};

	const ::gpk::array_pod<char_t>						requestPath						= requestReceived.Path;
	::gpk::tolower(requestPath);
	char												temp[1024]						= {};

	bool												won								= false;
	bool												blast							= false;
	::gpk::SCoord2<int32_t>								blastCoord						= {-1, -1};
	if(requestReceived.Method == ::gpk::HTTP_METHOD_GET) {
		if(::gpk::view_const_string{"start"}		== requestPath) {
			::gpk::view_const_string						ip								= {};
			::gpk::view_const_string						port							= {};
			::gpk::find("REMOTE_ADDR"	, environViews, ip);
			::gpk::find("REMOTE_IP"		, environViews, port);

			::gpk::view_const_string						boardWidth						= {};
			::gpk::view_const_string						boardHeigth						= {};
			::gpk::view_const_string						mineCount						= {};
			::gpk::find(::gpk::view_const_string{"width"	}, requestReceived.QueryStringKeyVals, boardWidth	);
			::gpk::find(::gpk::view_const_string{"height"	}, requestReceived.QueryStringKeyVals, boardHeigth	);
			::gpk::find(::gpk::view_const_string{"mines"	}, requestReceived.QueryStringKeyVals, mineCount	);
			::gpk::SCoord2<uint32_t>						boardMetrics					= {};
			::gpk::parseIntegerDecimal(boardWidth , &boardMetrics.x);
			::gpk::parseIntegerDecimal(boardHeigth, &boardMetrics.y);
			if( false == ::gpk::in_range(boardMetrics.x, 2U, 256U)
			 || false == ::gpk::in_range(boardMetrics.y, 2U, 256U)
			 ) {
				static constexpr	const char					responseFormat	[]				= "{ \"status\" : 400, \"description\" :\"Bad Request - Invalid board size: {%u, %u}.\" }\r\n";
				sprintf_s(temp, responseFormat, boardMetrics.x, boardMetrics.y);
				gpk_necall(output.append(::gpk::view_const_string{temp}), "%s", temp);
				return -1;
			}
			const uint32_t									totalCells						= boardMetrics.x * boardMetrics.y;
			uint32_t										countMines						= 0;
			::gpk::parseIntegerDecimal(mineCount, &countMines);

			if(0 == countMines || countMines > (totalCells / 2)) countMines
				= (totalCells > 500) ? (uint32_t)::gpk::max(1.0, boardMetrics.Length()) * 4
				: (totalCells > 200) ? (uint32_t)::gpk::max(1.0, boardMetrics.Length()) * 3
				: (totalCells > 100) ? (uint32_t)::gpk::max(1.0, boardMetrics.Length()) * 2
				: (uint32_t)::gpk::max(1.0, boardMetrics.Length())
				;

			gameState.Start(boardMetrics, countMines);
			gpk_necall(::gameStateId(strIdGame, ip, port), "%s", "Unknown error");
			idGame										= {strIdGame.begin(), strIdGame.size()};
			gpk_necall(::gameStateSave(gameState, idGame), "Cannot save file state. %s", "Unknown error");
		}
		else {
			static constexpr	const char					tempFormat		[]				= "{ \"status\" : 400, \"description\" :\"Bad Request - Invalid command: '%%.%us'.\" }\r\n";
			char											responseFormat	[4096]			= {};
			sprintf_s(responseFormat, tempFormat, requestPath.size());
			sprintf_s(temp, responseFormat, requestPath.begin());
			gpk_necall(output.append(::gpk::view_const_string{temp}), "%s", temp);
			return -1;
		}
	}
	else {
		::gpk::SJSONReader								requestCommands					= {};
		if errored(::gpk::jsonParse(requestCommands, {requestReceived.ContentBody.begin(), requestReceived.ContentBody.size()})) {
			static constexpr	const char					responseFormat	[]				= "{ \"status\" : 400, \"description\" :\"Bad Request - Content body received is not a valid application/json document. Error at position %u.\" }\r\n";
			sprintf_s(temp, responseFormat, requestCommands.StateRead.IndexCurrentChar);
			gpk_necall(output.append(::gpk::view_const_string{temp}), "%s", temp);
			return -1;
		}

		if errored(::gpk::jsonExpressionResolve("game_id", requestCommands, 0, idGame))	{
			static constexpr	const char					responseFormat	[]				= "{ \"status\" : 400, \"description\" :\"Bad Request - '%s' element not found in json document.\" }\r\n";
			sprintf_s(temp, responseFormat, "game_id");
			gpk_necall(output.append(::gpk::view_const_string{temp}), "%s", temp);
			return -1;
		}

		::gpk::view_const_string						action;
		::gpk::view_const_string						actionCellX;
		::gpk::view_const_string						actionCellY;
		::gpk::error_t									idxCommandNode						= ::gpk::jsonExpressionResolve("action.name", requestCommands, 0, action)		; (void)idxCommandNode;
		::gpk::error_t									idxActionCellX						= ::gpk::jsonExpressionResolve("action.x"	, requestCommands, 0, actionCellX)	; (void)idxActionCellX;
		::gpk::error_t									idxActionCellY						= ::gpk::jsonExpressionResolve("action.y"	, requestCommands, 0, actionCellY)	; (void)idxActionCellY;
		::gpk::SCoord2<uint32_t>						actionCellCoord						= {(uint32_t)-1, (uint32_t)-1};
		::gpk::parseIntegerDecimal(actionCellX, &actionCellCoord.x);
		::gpk::parseIntegerDecimal(actionCellY, &actionCellCoord.y);
		::gpk::error_t									isGameFinished						= ::gameStateLoad(gameState, idGame);
		if errored(isGameFinished)	{
			static constexpr	const char					tempFormat		[]				= "{ \"status\" : 400, \"description\" :\"Bad Request - Cannot load state for game_id: '%%.%us'.\" }\r\n";
			char											responseFormat	[4096]			= {};
			sprintf_s(responseFormat, tempFormat, idGame.size());
			sprintf_s(temp, responseFormat, idGame.begin());
			gpk_necall(output.append(::gpk::view_const_string{temp}), "%s", temp);
			return -1;
		}

		bool											validCell							= ::gpk::in_range(actionCellCoord, {{}, gameState.Board.metrics()});
		if(1 == isGameFinished)
			blast										= true;
		else if(2 == isGameFinished)
			won											= true;
		else {
				 if(::gpk::view_const_string{"continue"	}	== action) {}
			else if(::gpk::view_const_string{"flag"}		== action) { if(false == validCell) { output_error_invalid_cell(actionCellCoord, output); return -1; } gameState.Flag(actionCellCoord); ::gameStateSave(gameState, idGame); }
			else if(::gpk::view_const_string{"hold"}		== action) { if(false == validCell) { output_error_invalid_cell(actionCellCoord, output); return -1; } gameState.Hold(actionCellCoord); ::gameStateSave(gameState, idGame); }
			else if(::gpk::view_const_string{"step"}		== action) { if(false == validCell) { output_error_invalid_cell(actionCellCoord, output); return -1; }
				int32_t										dead								= gameState.Step(actionCellCoord);
				if(dead > 0)
					blast										= true;
				::gameStateSave(gameState, idGame);
			}
			else {
				static constexpr	const char					tempFormat		[]				= "{ \"status\" : 400, \"description\" :\"Bad Request - Invalid command: '%%.%us'.\" }\r\n";
				char											responseFormat	[4096]			= {};
				sprintf_s(responseFormat, tempFormat, requestPath.size());
				sprintf_s(temp, responseFormat, requestPath.begin());
				gpk_necall(output.append(::gpk::view_const_string{temp}), "%s", temp);
				return -1;
			}
		}
		if(blast) {
			gameState.GetBlast(actionCellCoord);
			blastCoord									= actionCellCoord.template Cast<int32_t>();
		}
	}

	::gpk::SImageMonochrome<uint64_t>					cellsMines; gpk_necall(cellsMines.resize(gameState.Board.metrics())	, "%s", "Out of memory?");
	::gpk::SImageMonochrome<uint64_t>					cellsFlags; gpk_necall(cellsFlags.resize(gameState.Board.metrics())	, "%s", "Out of memory?");
	::gpk::SImageMonochrome<uint64_t>					cellsWhats; gpk_necall(cellsWhats.resize(gameState.Board.metrics())	, "%s", "Out of memory?");
	::gpk::SImageMonochrome<uint64_t>					cellsHides; gpk_necall(cellsHides.resize(gameState.Board.metrics())	, "%s", "Out of memory?");
	::gpk::SImageMonochrome<uint64_t>					cellsSafes; gpk_necall(cellsSafes.resize(gameState.Board.metrics())	, "%s", "Out of memory?");
	const uint32_t										totalMines		= gameState.GetMines(cellsMines.View);
	const uint32_t										totalFlags		= gameState.GetFlags(cellsFlags.View);
	const uint32_t										totalWhats		= gameState.GetWhats(cellsWhats.View);
	const uint32_t										totalHides		= gameState.GetHides(cellsHides.View);
	const uint32_t										totalSafes		= gameState.GetSafes(cellsSafes.View);
	::gpk::SImage<uint8_t>								hints;
	gpk_necall(hints.resize(gameState.Board.metrics(), 0), "%s", "");
	gameState.GetHints(hints.View);
	if(totalHides <= totalMines && false == blast)
		won												= true;

//#define ROWS_AS_KEYS
	gpk_necall(output.append(::gpk::view_const_string{"{"})													, "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{"\"game_id\":"})										, "%s", "Out of memory?");
	gpk_necall(output.push_back('"')																		, "%s", "Out of memory?");
	gpk_necall(output.append(idGame)																		, "%s", "Out of memory?");
	gpk_necall(output.push_back('"')																		, "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{", \"dead\":"})										, "%s", "Out of memory?");
	gpk_necall(output.append(blast ? ::gpk::view_const_string{"true"}: ::gpk::view_const_string{"false"})	, "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{", \"won\":"})										, "%s", "Out of memory?");
	gpk_necall(output.append(won ? ::gpk::view_const_string{"true"}: ::gpk::view_const_string{"false"})		, "%s", "Out of memory?");
	if(blast) {
		sprintf_s(temp, ", \"blast\":{\"x\":%u,\"y\":%u}", blastCoord.x, blastCoord.y);
		gpk_necall(output.append(::gpk::view_const_string{temp}), "%s", "Out of memory?");
	}
	//else
	//	gpk_necall(output.push_back(',')																		, "%s", "Out of memory?");
#define MINESWEEPER_DEBUG
#if defined(MINESWEEPER_DEBUG)
	gpk_necall(output.append(::gpk::view_const_string{"\n,\"mines\":"})	, "%s", "Out of memory?"); gpk_necall(::output_board_generate(gameState.Board.metrics(), cellsMines.View, output), "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{"\n,\"flags\":"})	, "%s", "Out of memory?"); gpk_necall(::output_board_generate(gameState.Board.metrics(), cellsFlags.View, output), "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{"\n,\"whats\":"})	, "%s", "Out of memory?"); gpk_necall(::output_board_generate(gameState.Board.metrics(), cellsWhats.View, output), "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{"\n,\"hides\":"})	, "%s", "Out of memory?"); gpk_necall(::output_board_generate(gameState.Board.metrics(), cellsHides.View, output), "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{"\n,\"safes\":"})	, "%s", "Out of memory?"); gpk_necall(::output_board_generate(gameState.Board.metrics(), cellsSafes.View, output), "%s", "Out of memory?");


	gpk_necall(output.append(::gpk::view_const_string{"\n,\"hints\":"})	, "%s", "Out of memory?");
	gpk_necall(output.push_back('[')									, "%s", "Out of memory?");

	for(uint32_t row = 0; row < gameState.Board.metrics().y; ++row) {
		gpk_necall(output.push_back('\n'), "%s", "Out of memory?");
		gpk_necall(output.push_back('['), "%s", "Out of memory?");
		for(uint32_t x = 0; x < gameState.Board.metrics().x; ++x) {
			const uint8_t										cell							= hints[row][x];
			gpk_necall(output.push_back('0' + cell), "%s", "Out of memory?");
			if(x < (gameState.Board.metrics().x - 1))
				gpk_necall(output.push_back(','), "%s", "Out of memory?");
		}
		gpk_necall(output.push_back(']'), "%s", "Out of memory?");
		if(row < (gameState.Board.metrics().y - 1))
			gpk_necall(output.push_back(','), "%s", "Out of memory?");
	}
	gpk_necall(output.push_back('\n'), "%s", "Out of memory?");
	gpk_necall(output.push_back(']')									, "%s", "Out of memory?");
#endif
	gpk_necall(output.append(::gpk::view_const_string{"\n,\"board\":"})	, "%s", "Out of memory?");
	gpk_necall(output.push_back('[')									, "%s", "Out of memory?");

	const ::gpk::view_const_string					symbolBoom	= "\"B\"";
	const ::gpk::view_const_string					symbolWhat	= "\"?\"";
	const ::gpk::view_const_string					symbolFlag	= "\"!\"";
	const ::gpk::view_const_string					symbolHide	= "\"~\"";
	for(uint32_t y = 0; y < gameState.Board.metrics().y; ++y) {
		gpk_necall(output.push_back('\n'), "%s", "Out of memory?");
		gpk_necall(output.push_back('['), "%s", "Out of memory?");
		for(uint32_t x = 0; x < gameState.Board.metrics().x; ++x) {
			const ::btl::SMineSweeperCell						cellData							= gameState.Board[y][x];
			const uint8_t										cellHint							= hints[y][x];
			if(cellData.Boom)
				gpk_necall(output.append(symbolBoom), "%s", "Out of memory?");
			else if(cellData.Hide) {
				if(cellData.Flag)
					gpk_necall(output.append(symbolFlag), "%s", "Out of memory?");
				else if(cellData.What)
					gpk_necall(output.append(symbolWhat), "%s", "Out of memory?");
				else
					gpk_necall(output.append(symbolHide), "%s", "Out of memory?");
			}
			else {
				gpk_necall(output.push_back('"')			, "%s", "Out of memory?");
				if(cellHint > 0)
					gpk_necall(output.push_back('0' + cellHint)	, "%s", "Out of memory?");
				else
					gpk_necall(output.push_back(' ')	, "%s", "Out of memory?");
				gpk_necall(output.push_back('"')			, "%s", "Out of memory?");
			}
			if(x < (gameState.Board.metrics().x - 1))
				gpk_necall(output.push_back(','), "%s", "Out of memory?");
		}
		gpk_necall(output.push_back(']'), "%s", "Out of memory?");
		if(y < (gameState.Board.metrics().y - 1))
			gpk_necall(output.push_back(','), "%s", "Out of memory?");
	}
	gpk_necall(output.push_back('\n'), "%s", "Out of memory?");
	gpk_necall(output.push_back(']')									, "%s", "Out of memory?");

	gpk_necall(output.append(::gpk::view_const_string{"}"}), "%s", "Out of memory?");
	if(output.size()) {
		OutputDebugStringA(output.begin());
		OutputDebugStringA("\n");
	}
	return 0;
}
