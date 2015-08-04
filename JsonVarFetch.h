#ifndef JSON_VAR_FETCH_H
#define JSON_VAR_FETCH_H

//#define DEBUG
//#define INFO
//#define VERBOSE
//#define INSANE

#define UINT_S uint16_t
#define UINT_ uint16_t


// strlen strncmp memcpy
#include <string.h>

using namespace std;

// Tweaked these levels for the current SpaceAPI
// feeds.calendar.url (3)
#define QUERY_DEPTH           4
// 3 levels: contact.keymaster[0] -> stack 6
#define PARSER_STATE_COUNT   10
// feeds.calendar.url = "http://www.google.com/calendar/ical/f3j6egtm35u2v027rog3sob7gk%40group.calendar.google.com/public/basic.ics" -> 108
// issue_report_channels -> 21
#define VARIABLE_LENGTH     5

// This enables you to reduce the variable length even further (as long as the requested query per identifier is unique
#define ALLOW_TRUNCATED_IDENTIFIERS

// TODO: skip checkCompleteValue if complete or identifier doesn't match

namespace ParserState
{
    enum Enum : uint8_t
    {
        Uninitialized   = 245,
        Object          = 246,
        Array           = 247,
        Value           = 248,
        String          = 249,
        Pair            = 250,
        Identifier      = 251,
        Number          = 252,
        Special         = 253,
        NextSegment     = 254,
        Corrupt         = 255
    };
}

namespace ParseStatus
{
    enum Enum : int8_t
    {
        AllocationError       = -3,
        ParserError           = -2,
        JsonError             = -1,
        Ok                    =  0,
        Complete              =  1,
        CompletePartialResult =  2,
        CompleteFullResult    =  3
    };
}

class JsonVarFetch
{
    public:
        JsonVarFetch( const char* _arrQueries[], uint8_t _nQueries, char* _strResult, UINT_S _nMaxLen );
        ~JsonVarFetch();

    public:
        ParseStatus::Enum   processCharacter( char _c );

    private:
        const char**        m_arrQueries;
        uint8_t             m_nQueries;
        char*               m_strResult;
        UINT_S              m_nMaxLen;

        ParserState::Enum   m_eParserStates[ PARSER_STATE_COUNT ];
        uint8_t             m_nStateSize;
        char                m_strValue[ VARIABLE_LENGTH ];
        uint8_t             m_nIterator;
        UINT_S*             m_arrPathMatches;
        bool                m_bMatchingIdentifier;

    private:
        bool                _pushState( ParserState::Enum _eParserState );
        ParserState::Enum   _popState( );
        ParserState::Enum   _peekState( );
        ParserState::Enum   _peekState( bool _bListType );
        ParserState::Enum   _swapState( ParserState::Enum _eParserState );
        uint8_t             _getLevel( );
        inline bool         _isWhiteSpace( char _c );
        bool                _addCharacterToIdentifier( char _c );
        bool                _applyIteratorIdentifier( );
        bool                _checkCompleteIdentifier( );
        ParseStatus::Enum   _checkCompleteValue( );

#ifdef DEBUG
        void                _printDebugData( ParserState::Enum _eParserState );
        void                print( char* _strMessage );
#endif

};

#endif
