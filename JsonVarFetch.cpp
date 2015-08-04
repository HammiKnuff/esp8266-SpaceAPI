#ifdef ARDUINO
#include "Arduino.h"
#else
// strlen strncmp memcpy
#include <string.h>
#include <stdint.h>

// cout
#include "iostream";
#endif

#include "JsonVarFetch.h"

using namespace std;

/* A utility function to reverse a string  */
/*
// http://www.geeksforgeeks.org/implement-itoa/
void strreverse(char str[], int length)
{
    int start = 0;
    int end = length -1;
    char temp;
    while (start < end)
    {
        temp = *(str+start);
        *(str+start) = *(str+end);
        *(str+end) = temp;
        //swap(*(str+start), *(str+end));
        start++;
        end--;
    }
}


// Implementation of itoa()
char* itoa(int num, char* str, int base)
{
    int i = 0;
    bool isNegative = false;
 
    // Handle 0 explicitely, otherwise empty string is printed for 0
    if (num == 0)
    {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }
 
    // In standard itoa(), negative numbers are handled only with 
    // base 10. Otherwise numbers are considered unsigned.
    if (num < 0 && base == 10)
    {
        isNegative = true;
        num = -num;
    }
 
    // Process individual digits
    while (num != 0)
    {
        int rem = num % base;
        str[i++] = (rem > 9)? (rem-10) + 'a' : rem + '0';
        num = num/base;
    }
 
    // If number is negative, append '-'
    if (isNegative)
        str[i++] = '-';
 
    str[i] = '\0'; // Append string terminator
 
    // Reverse the string
    strreverse(str, i);
 
    return str;
}
*/



////////////////////////////////////////////////////////////////////////////////
// JsonVarFetch::JsonVarFetch
////////////////////////////////////////////////////////////////////////////////
JsonVarFetch::JsonVarFetch( const char* _arrQueries[], uint8_t _nQueries, char* _strResult, UINT_S _nMaxLen )
{
    // Store the data locally
    this->m_arrQueries  = _arrQueries;
    this->m_nQueries    = _nQueries;
    this->m_strResult   = _strResult;
    this->m_nMaxLen     = _nMaxLen;

#ifdef DEBUG
    // Initialize the array so it looks nicer when debugging
    for ( uint8_t nState = 0; nState < ( sizeof( this->m_eParserStates ) / sizeof( ParserState::Enum ) ); nState++ )
        this->m_eParserStates[ nState ] = ParserState::Corrupt;
#endif

    this->m_nStateSize = 0;
    this->_pushState( ParserState::Uninitialized );

    // Start with zero length string
    this->m_strValue[ 0 ] = 0;

    // Iterator at 0
    this->m_nIterator = 0;

    // Build the identifier list
    this->m_arrPathMatches = new UINT_S[ QUERY_DEPTH * _nQueries ];
    //this->m_arrPathMatches[ 0 ] = 0;

#ifdef DEBUG
    for ( uint8_t nState = 0; nState < QUERY_DEPTH * _nQueries; nState++ )
        m_arrPathMatches[ nState ] = 0;
#endif

    // Clear the given string so we know what space is unused
    for ( UINT_S nResult = 0; nResult < _nMaxLen; nResult++ )
        _strResult[ nResult ] = 0;

    //this->m_nPathMatchLength = 1;
    uint8_t* arrPathMatchLength = new uint8_t[ _nQueries ];

    // Handle all queries
    UINT_S nChar;
    for ( uint8_t nQuery = 0; nQuery < _nQueries; nQuery++ )
    {
        nChar = 0;
        this->m_arrPathMatches[ QUERY_DEPTH * nQuery ] = 0;
        arrPathMatchLength[ nQuery ] = 1;

        while ( nChar < 255 && _arrQueries[ nQuery ][ nChar ] )
        {
            switch ( _arrQueries[ nQuery ][ nChar ] )
            {
                // Separator
                case '.':
                case '[':
                    // Start counting after the dot or index bracket
                    this->m_arrPathMatches[ QUERY_DEPTH * nQuery + arrPathMatchLength[ nQuery ] ] = nChar + 1;
                    arrPathMatchLength[ nQuery ]++;
                    break;
            }
            nChar++;

            // Terminate the last item with a 0 so we can determine its length
            this->m_arrPathMatches[ QUERY_DEPTH * nQuery + arrPathMatchLength[ nQuery ] ] = 0;
        }
    }

    delete [] arrPathMatchLength;

    // Remember if the identifier we built is a full match for (one of) our quer{y|ies}
    m_bMatchingIdentifier = false;
};

JsonVarFetch::~JsonVarFetch( )
{
    delete [] this->m_arrPathMatches;
};

ParseStatus::Enum JsonVarFetch::processCharacter( char _c )
{
    UINT_S nValueLen;
    ParseStatus::Enum eParseStatus;

    switch ( this->_peekState( ) )
    {
        case ParserState::Uninitialized:
            // New JSON object, allow 'whitespace' but expect '{'
            if ( _isWhiteSpace( _c ) )
                return ParseStatus::Ok;

            if ( _c != '{' )
                return ParseStatus::JsonError;

            // JSON starts as an object
            this->_pushState( ParserState::Object );
            return ParseStatus::Ok;

        case ParserState::Object:
            // Object, allow 'whitespace' but expect '\"'
            if ( _isWhiteSpace( _c ) )
                return ParseStatus::Ok;

            if ( _c != '\"' )
                return ParseStatus::JsonError;

            // Start of pair
            this->_pushState( ParserState::Pair );
            return ParseStatus::Ok;

        case ParserState::Array:
            // Should not reach this (Array is always immediately followed by Value on the stack
            return ParseStatus::ParserError;

        case ParserState::Value:
            // Value, allow 'whitespace'
            if ( _isWhiteSpace( _c ) )
                return ParseStatus::Ok;

            // Determine what incoming character we have
            switch ( _c )
            {
                case '{':
                    this->_swapState( ParserState::Object );
                    return ParseStatus::Ok;

                case '[':
                    // Pop the Value state
                    this->_popState( );

                    // Push the current iterator
                    // NOTE: this is somewhat ugly
                    this->_pushState( (ParserState::Enum)this->m_nIterator );

                    // Push array type on the stack, followed by Value
                    this->_pushState( ParserState::Array );
                    this->_pushState( ParserState::Value );

                    // Reset the iterator
                    this->m_nIterator = 0;
                    this->_applyIteratorIdentifier();
#ifdef DEBUG
                    this->_printDebugData( ParserState::Identifier );
#endif
                    // Match identifier with the current identifiers
                    this->_checkCompleteIdentifier();
                    this->m_strValue[ 0 ] = 0;

                    return ParseStatus::Ok;

                case '\"':
                    this->_swapState( ParserState::String );
                    return ParseStatus::Ok;

                case 't': // true
                case 'f': // false
                case 'n': // null
                    // Add special value to the identifier string
                    // TODO: only if complete and matching identifier
                    if ( m_bMatchingIdentifier && !this->_addCharacterToIdentifier( _c ) )
                        return ParseStatus::AllocationError;

                    this->_swapState( ParserState::Special );
                    return ParseStatus::Ok;

                case '-':
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                    // Add value to the identifier string
                    // TODO: only if complete and matching identifier
                    if ( m_bMatchingIdentifier && !this->_addCharacterToIdentifier( _c ) )
                        return ParseStatus::AllocationError;

                    // Switch to number mode
                    this->_swapState( ParserState::Number );
                    return ParseStatus::Ok;

                default:
                    return ParseStatus::ParserError;
            }

        case ParserState::String:
            if ( _c == '\"' )
            {
#ifdef DEBUG
                this->_printDebugData( this->_peekState( ) );
#endif
                // If complete
                eParseStatus = this->_checkCompleteValue( );
                if ( eParseStatus < ParseStatus::Ok )
                    return eParseStatus;
                this->m_strValue[ 0 ] = 0;

                // String complete
                this->_swapState( ParserState::NextSegment );
                return ParseStatus::Ok;
            }

            // Add the string character to the identifier
            // TODO: only if complete and matching identifier
            if ( m_bMatchingIdentifier && !this->_addCharacterToIdentifier( _c ) )
                return ParseStatus::AllocationError;

            return ParseStatus::Ok;

        case ParserState::Pair:
            if ( _c == '\"' )
            {
#ifdef DEBUG
                this->_printDebugData( ParserState::Identifier );
#endif

                // Match identifier with the current identifiers
                this->_checkCompleteIdentifier();
                this->m_strValue[ 0 ] = 0;

                // Pair identifier complete
                this->_swapState( ParserState::Identifier );
                return ParseStatus::Ok;
            }

            // Add the character to the identifier string
#ifdef ALLOW_TRUNCATED_IDENTIFIERS
            this->_addCharacterToIdentifier( _c );
#else
            if ( !this->_addCharacterToIdentifier( _c ) )
                return ParseStatus::AllocationError;
#endif
            return ParseStatus::Ok;

        case ParserState::Identifier:
            // Identifier waiting on colon, allow 'whitespace'
            if ( _isWhiteSpace( _c ) )
                return ParseStatus::Ok;

            // No colon; this should not happen
            if ( _c != ':' )
                return ParseStatus::JsonError;

            this->_swapState( ParserState::Value );
            return ParseStatus::Ok;

        case ParserState::Number:
            // (-?)(0|[1-9][0-9]+)(\.[0-9]+)?
            // TODO

            /*
            // Whitespace finishes value, ready for next segment
            if ( _isWhiteSpace( _c ) )
            {
                this->_swapState( ParserState::NextSegment );
                return ParseStatus::Ok;
            }
            */

            switch ( _c )
            {
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                case '.':
                    // TODO: only if complete and matching identifier
                    if ( m_bMatchingIdentifier && !this->_addCharacterToIdentifier( _c ) )
                        return ParseStatus::AllocationError;

                    return ParseStatus::Ok;

                case ',':
                    // Immediate next value/pair
#ifdef DEBUG
                    this->_printDebugData( this->_peekState( ) );
#endif
                    // If complete
                    eParseStatus = this->_checkCompleteValue( );
                    if ( eParseStatus < ParseStatus::Ok )
                        return eParseStatus;
                    this->m_strValue[ 0 ] = 0;

                    this->_popState( );
                    switch ( this->_peekState( ) )
                    {
                        case ParserState::Object:
                            // If we are iterating an Object, do nothing so we can accept Pair mode
                            return ParseStatus::Ok;

                        case ParserState::Array:
                            // If we are iterating an Array, increase the iterator and switch to Value mode
                            // Increase the iterator in case of Array
                            this->m_nIterator++;

                            /*
                            // NOTE: Starting 245 (uninitialized), the iterator overlaps and it migh confuse one
                            if ( this->m_nIterator >= ParserState::Uninitialized )
                                return ParseStatus::AllocationError;
                            */
                            this->_applyIteratorIdentifier();
#ifdef DEBUG
                            this->_printDebugData( ParserState::Identifier );
#endif
                            // Match identifier with the current identifiers
                            this->_checkCompleteIdentifier();
                            this->m_strValue[ 0 ] = 0;

                            this->_pushState( ParserState::Value );
                            return ParseStatus::Ok;

                        default:
                            return ParseStatus::JsonError;
                    }

                case '}':
                    // Tear down the stack (object)
#ifdef DEBUG
                    this->_printDebugData( this->_peekState( ) );
#endif
                    // If complete
                    eParseStatus = this->_checkCompleteValue( );
                    if ( eParseStatus < ParseStatus::Ok )
                        return eParseStatus;
                    this->m_strValue[ 0 ] = 0;

                    // Pop Number
                    this->_popState( );

                    // Pop Object
                    switch ( this->_popState( ) )
                    {
                        case ParserState::Object:
                            // End of array, ready for a new segment (on its parent)
                            this->_pushState( ParserState::NextSegment );
                            return ParseStatus::Ok;

                        default:
                            return ParseStatus::JsonError;
                    }

                case ']':
                    // Tear down the stack (array)
#ifdef DEBUG
                    this->_printDebugData( this->_peekState( ) );
#endif
                    // If complete
                    eParseStatus = this->_checkCompleteValue( );
                    if ( eParseStatus < ParseStatus::Ok )
                        return eParseStatus;
                    this->m_strValue[ 0 ] = 0;

                    // Pop Number
                    this->_popState( );

                    // Pop Array
                    switch ( this->_popState( ) )
                    {
                        case ParserState::Array:
                            // End of array, ready for a new segment (on its parent)
                            // Pop the previous iterator
                            // NOTE: this is somewhat ugly
                            this->m_nIterator = (uint8_t)this->_popState( );

                            this->_pushState( ParserState::NextSegment );
                            return ParseStatus::Ok;

                        default:
                            return ParseStatus::JsonError;
                    }

                case ' ':
                case '\t':
                case '\n':
                case '\r':
                    // Whitespace finishes value, ready for next segment
#ifdef DEBUG
                    this->_printDebugData( this->_peekState( ) );
#endif
                    // If complete
                    eParseStatus = this->_checkCompleteValue( );
                    if ( eParseStatus < ParseStatus::Ok )
                        return eParseStatus;
                    this->m_strValue[ 0 ] = 0;

                    this->_swapState( ParserState::NextSegment );
                    return ParseStatus::Ok;

                default:
                    return ParseStatus::JsonError;
            }

        case ParserState::Special:
            // TODO

            /*
            // Whitespace finishes value, ready for next segment
            if ( _isWhiteSpace( _c ) )
            {
                this->_swapState( ParserState::NextSegment );
                return ParseStatus::Ok;
            }
            */

            switch ( _c )
            {
                case 'r':
                case 'u':
                case 'e':
                case 'a':
                case 'l':
                case 's':
                    // Add special value to the identifier string
                    // TODO: only if complete and matching identifier
                    if ( m_bMatchingIdentifier && !this->_addCharacterToIdentifier( _c ) )
                        return ParseStatus::AllocationError;

                    return ParseStatus::Ok;

                case ',':
                    // Immediate next value/pair
#ifdef DEBUG
                    this->_printDebugData( this->_peekState( ) );
#endif
                    // If complete
                    eParseStatus = this->_checkCompleteValue( );
                    if ( eParseStatus < ParseStatus::Ok )
                        return eParseStatus;
                    this->m_strValue[ 0 ] = 0;

                    this->_popState( );
                    switch ( this->_peekState( ) )
                    {
                        case ParserState::Object:
                            // If we are iterating an Object, do nothing so we can accept Pair mode
                            return ParseStatus::Ok;

                        case ParserState::Array:
                            // If we are iterating an Array, increase the iterator and switch to Value mode
                            // Increase the iterator in case of Array
                            this->m_nIterator++;

                            /*
                            // NOTE: Starting 245 (uninitialized), the iterator overlaps and it migh confuse one
                            if ( this->m_nIterator >= ParserState::Uninitialized )
                                return ParseStatus::AllocationError;
                            */
                            this->_applyIteratorIdentifier();
#ifdef DEBUG
                            this->_printDebugData( ParserState::Identifier );
#endif
                            // TODO: match identifier with the current identifiers
                            this->_checkCompleteIdentifier();
                            this->m_strValue[ 0 ] = 0;

                            this->_pushState( ParserState::Value );
                            return ParseStatus::Ok;

                        default:
                            return ParseStatus::JsonError;
                    }

                case '}':
                    // Tear down the stack (object)
#ifdef DEBUG
                    this->_printDebugData( this->_peekState( ) );
#endif
                    // If complete
                    eParseStatus = this->_checkCompleteValue( );
                    if ( eParseStatus < ParseStatus::Ok )
                        return eParseStatus;
                    this->m_strValue[ 0 ] = 0;

                    // Pop Number
                    this->_popState( );

                    // Pop Object
                    switch ( this->_popState( ) )
                    {
                        case ParserState::Object:
                            // End of array, ready for a new segment (on its parent)
                            this->_pushState( ParserState::NextSegment );
                            return ParseStatus::Ok;

                        default:
                            return ParseStatus::JsonError;
                    }

                case ']':
                    // Tear down the stack (array)
#ifdef DEBUG
                    this->_printDebugData( this->_peekState( ) );
#endif
                    // If complete
                    eParseStatus = this->_checkCompleteValue( );
                    if ( eParseStatus < ParseStatus::Ok )
                        return eParseStatus;
                    this->m_strValue[ 0 ] = 0;

                    // Pop Number
                    this->_popState( );

                    // Pop Array
                    switch ( this->_popState( ) )
                    {
                        case ParserState::Array:
                            // Pop the previous iterator
                            // NOTE: this is somewhat ugly
                            this->m_nIterator = (uint8_t)this->_popState( );

                            // End of array, ready for a new segment (on its parent)
                            this->_pushState( ParserState::NextSegment );
                            return ParseStatus::Ok;

                        default:
                            return ParseStatus::JsonError;
                    }

                case ' ':
                case '\t': // tab
                case '\n': // newline
                case '\r': // return
                    // Whitespace finishes value, ready for next segment
#ifdef DEBUG
                    this->_printDebugData( this->_peekState( ) );
#endif
                    // If complete
                    eParseStatus = this->_checkCompleteValue( );
                    if ( eParseStatus < ParseStatus::Ok )
                        return eParseStatus;
                    this->m_strValue[ 0 ] = 0;

                    this->_swapState( ParserState::NextSegment );
                    return ParseStatus::Ok;

                default:
                    return ParseStatus::JsonError;
            }

        case ParserState::NextSegment:
            if ( _isWhiteSpace( _c ) )
                return ParseStatus::Ok;

            // Determine what incoming character we have
            switch ( _c )
            {
                case '}':
                    // Tear down the stack (object)
                    // Pop NextSegment
                    this->_popState( );

                    // Pop Object
                    switch ( this->_popState( ) )
                    {
                        case ParserState::Object:
                            // End of array, ready for a new segment (on its parent)
                            this->_pushState( ParserState::NextSegment );
                            return ParseStatus::Ok;

                        default:
                            return ParseStatus::JsonError;
                    }

                case ']':
                    // Tear down the stack (array)
                    // Pop NextSegment
                    this->_popState( );

                    // Pop Array
                    switch ( this->_popState( ) )
                    {
                        case ParserState::Array:
                            // Pop the previous iterator
                            // NOTE: this is somewhat ugly
                            this->m_nIterator = (uint8_t)this->_popState( );

                            // End of array, ready for a new segment (on its parent)
                            this->_pushState( ParserState::NextSegment );
                            return ParseStatus::Ok;

                        default:
                            return ParseStatus::JsonError;
                    }

                case ',':
                    // Next value/pair
                    this->_popState( );
                    switch ( this->_peekState( ) )
                    {
                        case ParserState::Object:
                            // If we are iterating an Object, do nothing so we can accept Pair mode
                            return ParseStatus::Ok;

                        case ParserState::Array:
                            // If we are iterating an Array, increase the iterator and switch to Value mode
                            // Increase the iterator in case of Array
                            this->m_nIterator++;

                            /*
                            // NOTE: Starting 245 (uninitialized), the iterator overlaps and it migh confuse one
                            if ( this->m_nIterator >= ParserState::Uninitialized )
                                return ParseStatus::AllocationError;
                            */
                            this->_applyIteratorIdentifier();
#ifdef DEBUG
                            this->_printDebugData( ParserState::Identifier );
#endif
                            // TODO: match identifier with the current identifiers
                            this->_checkCompleteIdentifier();
                            this->m_strValue[ 0 ] = 0;

                            this->_pushState( ParserState::Value );

                            return ParseStatus::Ok;

                        default:
                            return ParseStatus::JsonError;
                    }

                default:
                    return ParseStatus::ParserError;
            }

            return ParseStatus::Ok;

        default:
            return ParseStatus::ParserError;
    }
};


bool JsonVarFetch::_pushState( ParserState::Enum _eParserState )
{
    // Check for stack overflow
    if ( this->m_nStateSize >= ( sizeof( this->m_eParserStates ) / sizeof( ParserState::Enum ) ) )
        return false;

    // Push element success
    this->m_eParserStates[ this->m_nStateSize++ ] = _eParserState;
    return true;
};

ParserState::Enum JsonVarFetch::_popState( )
{
    // Stack corruption?
    if ( this->m_nStateSize == 0 )
        return ParserState::Corrupt;

#ifdef DEBUG
    ParserState::Enum eOldState = this->m_eParserStates[ --this->m_nStateSize ];
    this->m_eParserStates[ this->m_nStateSize ] = ParserState::Corrupt;
    return eOldState;
#endif

    return this->m_eParserStates[ --this->m_nStateSize ];
};

ParserState::Enum JsonVarFetch::_peekState( )
{
    // Stack corruption
    if ( this->m_nStateSize == 0 )
        return ParserState::Corrupt;

    return this->m_eParserStates[ this->m_nStateSize - 1 ];
};

ParserState::Enum JsonVarFetch::_peekState( bool _bListType )
{
    if ( this->m_nStateSize == 0 )
        return ParserState::Corrupt;

    // Give back the parser state enum, or bubble down up to a list type element
    if ( _bListType )
    {
        for ( uint8_t nState = this->m_nStateSize - 1; nState > 0; nState-- )
        {
            switch ( this->m_eParserStates[ nState ] )
            {
                case ParserState::Object:
                    return ParserState::Object;
                case ParserState::Array:
                    return ParserState::Array;
            }
        }

        // Stack corruption?
        return ParserState::Corrupt;
    } else {
        return this->m_eParserStates[ this->m_nStateSize - 1 ];
    }
};

ParserState::Enum JsonVarFetch::_swapState( ParserState::Enum _eParserState )
{
    ParserState::Enum eOldState;
    // Stack corruption?
    if ( this->m_nStateSize == 0 )
        return ParserState::Corrupt;

    eOldState = this->m_eParserStates[ this->m_nStateSize - 1 ];
    this->m_eParserStates[ this->m_nStateSize - 1 ] = _eParserState;

    return eOldState;
};

uint8_t JsonVarFetch::_getLevel( )
{
    uint8_t nLevel = 0;

    for ( uint8_t nState = this->m_nStateSize - 1; nState > 0; nState-- )
    {
        switch ( this->m_eParserStates[ nState ] )
        {
            case ParserState::Object:
            case ParserState::Array:
                nLevel++;
        }
    }

    return nLevel;
};

inline bool JsonVarFetch::_isWhiteSpace( char _c )
{
    switch ( _c )
    {
        case ' ':
        case '\t':
        case '\n':
        case '\r':
            return true;

        default:
            return false;
    }
};

bool JsonVarFetch::_addCharacterToIdentifier( char _c )
{
    UINT_S nValueLen = strlen( this->m_strValue );

    // Check if we have room for this character
    if ( nValueLen >= sizeof( this->m_strValue ) )
        return false;

    // Add character and string termination
    this->m_strValue[ nValueLen++ ] = _c;
    this->m_strValue[ nValueLen ] = 0;
    return true;
};

bool JsonVarFetch::_applyIteratorIdentifier( )
{
    // Cheesy solution
    if ( this->m_nIterator > 99999999999999 || this->m_nIterator < -9999999999999 )
        return false;

    // Do the actual conversion
    itoa( this->m_nIterator, this->m_strValue, 10 );

    return true;
};

bool JsonVarFetch::_checkCompleteIdentifier( )
{
    UINT_S nMatchStartPos;
    uint8_t nCurrentLevel = this->_getLevel( ) - 1;
    UINT_S nValueLength = strlen( this->m_strValue );

    // Assume we have no matches on this complete identifier
    m_bMatchingIdentifier = false;

    // Check if the amout of pathmatches is the same as the level - 1 we have
    for ( uint8_t nQuery = 0; nQuery < this->m_nQueries; nQuery++ )
    {
        // Get the index determined upon initialization time
        nMatchStartPos = this->m_arrPathMatches[ QUERY_DEPTH * nQuery + nCurrentLevel ];

        // Continue to the next item if the value is already found
        if ( nMatchStartPos == 255 )
        {
            // If the next is 0: it is a full match, flag
            if ( nCurrentLevel < QUERY_DEPTH && this->m_arrPathMatches[ QUERY_DEPTH * nQuery + nCurrentLevel + 1 ] == 0 )
                m_bMatchingIdentifier = true;

            continue;
        }

        if ( nCurrentLevel && this->m_arrPathMatches[ QUERY_DEPTH * nQuery + nCurrentLevel - 1 ] != 255 )
            continue;

        UINT_S nQueryPartLength = strlen( this->m_arrQueries[ nQuery ] + nMatchStartPos );
        if ( nQueryPartLength < nValueLength )
            continue;

        if ( ( nQueryPartLength > nValueLength )
          && ( this->m_arrQueries[ nQuery ][ nMatchStartPos + nValueLength ] != '.' )
          && ( this->m_arrQueries[ nQuery ][ nMatchStartPos + nValueLength ] != '[' )
          && ( this->m_arrQueries[ nQuery ][ nMatchStartPos + nValueLength ] != ']' ) )
            continue;

        if ( strncmp( this->m_strValue, this->m_arrQueries[ nQuery ] + nMatchStartPos, nValueLength ) )
            continue;

        this->m_arrPathMatches[ QUERY_DEPTH * nQuery + nCurrentLevel ] = 255;

        // If the next is 0: it is a full match, flag
        if ( nCurrentLevel < QUERY_DEPTH && this->m_arrPathMatches[ QUERY_DEPTH * nQuery + nCurrentLevel + 1 ] == 0 )
            m_bMatchingIdentifier = true;

    }

    // No match
    return false;
};

ParseStatus::Enum JsonVarFetch::_checkCompleteValue( )
{
    UINT_S nMatchStartPos;
    uint8_t nCurrentLevel = this->_getLevel( ) - 1;
    UINT_S nValueLength = strlen( this->m_strValue );

    uint8_t nMatches = 0;

    // No length means: nothing to edit
    if ( !nValueLength )
        return ParseStatus::Ok;

    // Check if the amout of pathmatches is the same as the level - 1 we have
    for ( uint8_t nQuery = 0; nQuery < this->m_nQueries; nQuery++ )
    {
        // Already completed
        if ( this->m_arrPathMatches[ QUERY_DEPTH * nQuery + nCurrentLevel ] != 255 )
            continue;

        // This identifier is not our target: we have to go deeper, so skip for now
        if ( nCurrentLevel < QUERY_DEPTH && this->m_arrPathMatches[ QUERY_DEPTH * nQuery + nCurrentLevel + 1 ] != 0 )
            continue;

        // Clear the query so we don't match it again
        for ( uint8_t nPathPart = 0; nPathPart < nCurrentLevel; nCurrentLevel++ )
        {
            this->m_arrPathMatches[ QUERY_DEPTH * nQuery + nPathPart + 1 ] = 0;
        }

        // Set the n'th string; make room if we have to
        UINT_S nStringOffset = 0;
        UINT_S nCurrentResultLength;
        for ( UINT_S nStringIndex = 0; nStringIndex < nQuery; nStringIndex++ )
        {
            nCurrentResultLength = strlen( this->m_strResult + nStringOffset );
            nStringOffset += ( nCurrentResultLength + 1 );
        }

        // Check if the current value fits in the given space
        if ( nStringOffset + nValueLength >= this->m_nMaxLen )
        {
#ifdef DEBUG
            print( "offset + valuelength > maxlen!\n" );
#endif
            return ParseStatus::AllocationError;
        }

        // See if we need to shift stuff around
        // add x, len 7
        // [_yyyyyyyyyy          0]
        // [_yyyyyyyyyy   -------0]
        // [       _yyyyyyyyyy   0]
        // [xxxxxxx_yyyyyyyyyy   0]

        // [_yyyyyyyyyy_zzzzzz   0]
        // [_yyyyyyyyyy_zz!-zz   0]

        // [xxxxxxx_yyyyyyyyyy_zzz]zzz

        // Make sure the last nValueLength bytes are 0
        for ( UINT_S nResult = this->m_nMaxLen - nValueLength; nResult < this->m_nMaxLen; nResult++ )
        {
            if ( this->m_strResult[ nResult ] != 0 )
            {
#ifdef DEBUG
                print( "end padding not empty!\n" );
#endif
                return ParseStatus::AllocationError;
            }
        }
/*

b JsonVarFetch.cpp:953
b JsonVarFetch.cpp:965
r
x/66xb this->m_strResult 
s

*/
        for ( UINT_S nResult = this->m_nMaxLen - nValueLength - 2; nResult >= nValueLength - 1; nResult-- )
        {
            // Shift the characters
            this->m_strResult[ nResult + nValueLength ] = this->m_strResult[ nResult ];
        }

        // Copy over the value including terminating zero
        memcpy( this->m_strResult + nStringOffset, this->m_strValue, nValueLength + 1 );
    }

    // Success, TODO: check the number of results..
    //if ( !nMatches )
    return ParseStatus::Ok;
};

#ifdef DEBUG
void JsonVarFetch::_printDebugData( ParserState::Enum _eParserState )
{
#if defined(INFO) || defined(VERBOSE)

    char strNumber[ 10 ];

    print( "strValue: [" );
    print( this->m_strValue );
    print( "]\n" );

    print( "iterator: " );
    itoa( m_nIterator, strNumber, 10 );
    print( strNumber );
    print( "\n" );

    print( "level (stack): " );
    itoa( this->_getLevel( ), strNumber, 10 );
    print( strNumber );
    print( " (" );
    itoa( m_nStateSize, strNumber, 10 );
    print( strNumber );
    print( ")\n" );

    print( "ParserState: " );
    switch ( _eParserState )
    {
        case ParserState::Uninitialized:
            print( "Uninitialized\n" );
            break;

        case ParserState::Object:
            print( "Object\n" );
            break;

        case ParserState::Array:
            print( "Array\n" );
            break;

        case ParserState::Value:
            print( "Value\n" );
            break;

        case ParserState::String:
            print( "String\n" );
            break;

        case ParserState::Pair:
            print( "Pair\n" );
            break;

        case ParserState::Identifier:
            print( "Identifier\n" );
            break;

        case ParserState::Number:
            print( "Number\n" );
            break;

        case ParserState::Special:
            print( "Special\n" );
            break;

        case ParserState::NextSegment:
            print( "NextSegment\n" );
            break;

        case ParserState::Corrupt:
            print( "Corrupt\n" );
            break;

        default:
            print( "Uninitialized\n" );
            break;
    }

    print( "\n" );

#endif
};

void JsonVarFetch::print( char* _strMessage )
{
#ifdef ARDUINO
    Serial.print( _strMessage );
#else
    cout << _strMessage;
#endif
};

#endif

