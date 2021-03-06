/**********************************************

License: BSD
Project Webpage: http://cajun-jsonapi.sourceforge.net/
Author: Terry Caton

***********************************************/
/*
* Copyright (C) 2014 James Higley

Changed from std:string to std:wstring
*/

#include <cassert>
#include <set>
#include <sstream>

/*  

TODO:
* better documentation
* unicode character decoding

*/

namespace json
{


   inline std::wistream& operator >> (std::wistream& istr, UnknownElement& elementRoot) {
   Reader::Read(elementRoot, istr);
   return istr;
}

inline Reader::Location::Location() :
   m_nLine(0),
   m_nLineOffset(0),
   m_nDocOffset(0)
{}


//////////////////////
// Reader::InputStream

class Reader::InputStream // would be cool if we could inherit from std::istream & override "get"
{
public:
   InputStream(std::wistream& iStr) :
      m_iStr(iStr) {}

   // protect access to the input stream, so we can keeep track of document/line offsets
   wchar_t Get(); // big, define outside
   wchar_t Peek() {
      assert(m_iStr.eof() == false); // enforce reading of only valid stream data 
      return m_iStr.peek();
   }

   bool EOS() {
      m_iStr.peek(); // apparently eof flag isn't set until a character read is attempted. whatever.
      return m_iStr.eof();
   }

   const Location& GetLocation() const { return m_Location; }

private:
   std::wistream& m_iStr;
   Location m_Location;
};


inline wchar_t Reader::InputStream::Get()
{
   assert(m_iStr.eof() == false); // enforce reading of only valid stream data 
   wchar_t c = m_iStr.get();
   
   ++m_Location.m_nDocOffset;
   if (c == L'\n') {
      ++m_Location.m_nLine;
      m_Location.m_nLineOffset = 0;
   }
   else {
      ++m_Location.m_nLineOffset;
   }

   return c;
}



//////////////////////
// Reader::TokenStream

class Reader::TokenStream
{
public:
   TokenStream(const Tokens& tokens);

   const Token& Peek();
   const Token& Get();

   bool EOS() const;

private:
   const Tokens& m_Tokens;
   Tokens::const_iterator m_itCurrent;
};


inline Reader::TokenStream::TokenStream(const Tokens& tokens) :
   m_Tokens(tokens),
   m_itCurrent(tokens.begin())
{}

inline const Reader::Token& Reader::TokenStream::Peek() {
   assert(m_itCurrent != m_Tokens.end());
   return *(m_itCurrent); 
}

inline const Reader::Token& Reader::TokenStream::Get() {
   assert(m_itCurrent != m_Tokens.end());
   return *(m_itCurrent++); 
}

inline bool Reader::TokenStream::EOS() const {
   return m_itCurrent == m_Tokens.end(); 
}

///////////////////
// Reader (finally)


inline void Reader::Read(Object& object, std::wistream& istr)                { Read_i(object, istr); }
inline void Reader::Read(Array& array, std::wistream& istr)                  { Read_i(array, istr); }
inline void Reader::Read(String& string, std::wistream& istr)                { Read_i(string, istr); }
inline void Reader::Read(Number& number, std::wistream& istr)                { Read_i(number, istr); }
inline void Reader::Read(Boolean& boolean, std::wistream& istr)              { Read_i(boolean, istr); }
inline void Reader::Read(Null& null, std::wistream& istr)                    { Read_i(null, istr); }
inline void Reader::Read(UnknownElement& unknown, std::wistream& istr)       { Read_i(unknown, istr); }


template <typename ElementTypeT>   
void Reader::Read_i(ElementTypeT& element, std::wistream& istr)
{
   Reader reader;

   Tokens tokens;
   InputStream inputStream(istr);
   reader.Scan(tokens, inputStream);

   TokenStream tokenStream(tokens);
   reader.Parse(element, tokenStream);

   if (tokenStream.EOS() == false)
   {
      const Token& token = tokenStream.Peek();
      std::string str;
      size_t len = token.sValue.length();
      if (len > 0)
      {
          char* w = new char[len + 1];
          size_t s = wcstombs(w, token.sValue.c_str(), len);
          if (s != len)
              w[0] = 0;
          else
              w[s] = 0;
          str = std::string(w);
          delete[] w;
      }
      std::string sMessage = "Expected End of token stream; found " + str;
      throw ParseException(sMessage, token.locBegin, token.locEnd);
   }
}


inline void Reader::Scan(Tokens& tokens, InputStream& inputStream)
{
   while (EatWhiteSpace(inputStream),              // ignore any leading white space...
          inputStream.EOS() == false) // ...before checking for EOS
   {
      // if all goes well, we'll create a token each pass
      Token token;
      token.locBegin = inputStream.GetLocation();

      // gives us null-terminated string
      std::wstring sChar;
      sChar.push_back(inputStream.Peek());

      switch (sChar[0])
      {
         case L'{':
            token.sValue = sChar[0];
            MatchExpectedString(sChar, inputStream);
            token.nType = Token::TOKEN_OBJECT_BEGIN;
            break;

         case L'}':
            token.sValue = sChar[0];
            MatchExpectedString(sChar, inputStream);
            token.nType = Token::TOKEN_OBJECT_END;
            break;

         case L'[':
            token.sValue = sChar[0];
            MatchExpectedString(sChar, inputStream);
            token.nType = Token::TOKEN_ARRAY_BEGIN;
            break;

         case L']':
            token.sValue = sChar[0];
            MatchExpectedString(sChar, inputStream);
            token.nType = Token::TOKEN_ARRAY_END;
            break;

         case L',':
            token.sValue = sChar[0];
            MatchExpectedString(sChar, inputStream);
            token.nType = Token::TOKEN_NEXT_ELEMENT;
            break;

         case L':':
            token.sValue = sChar[0];
            MatchExpectedString(sChar, inputStream);
            token.nType = Token::TOKEN_MEMBER_ASSIGN;
            break;

         case L'"':
            MatchString(token.sValue, inputStream);
            token.nType = Token::TOKEN_STRING;
            break;

         case L'-':
         case L'0':
         case L'1':
         case L'2':
         case L'3':
         case L'4':
         case L'5':
         case L'6':
         case L'7':
         case L'8':
         case L'9':
            MatchNumber(token.sValue, inputStream);
            token.nType = Token::TOKEN_NUMBER;
            break;

         case L't':
            token.sValue = L"true";
            MatchExpectedString(token.sValue, inputStream);
            token.nType = Token::TOKEN_BOOLEAN;
            break;

         case L'f':
            token.sValue = L"false";
            MatchExpectedString(token.sValue, inputStream);
            token.nType = Token::TOKEN_BOOLEAN;
            break;

         case L'n':
            token.sValue = L"null";
            MatchExpectedString(token.sValue, inputStream);
            token.nType = Token::TOKEN_NULL;
            break;

         default: {
             std::string str;
             size_t len = sChar.length();
             if (len > 0)
             {
                 char* w = new char[len + 1];
                 size_t s = wcstombs(w, sChar.c_str(), len);
                 if (s != len)
                     w[0] = 0;
                 else
                     w[s] = 0;
                 str = std::string(w);
                 delete[] w;
             }
             std::string sErrorMessage = "Unexpected character in stream: " + str;
            throw ScanException(sErrorMessage, inputStream.GetLocation());
         }
      }

      token.locEnd = inputStream.GetLocation();
      tokens.push_back(token);
   }
}


inline void Reader::EatWhiteSpace(InputStream& inputStream)
{
   while (inputStream.EOS() == false && 
          ::isspace(inputStream.Peek()))
      inputStream.Get();
}

inline void Reader::MatchExpectedString(const std::wstring& sExpected, InputStream& inputStream)
{
   std::wstring::const_iterator it(sExpected.begin()),
                               itEnd(sExpected.end());
   for ( ; it != itEnd; ++it) {
      if (inputStream.EOS() ||      // did we reach the end before finding what we're looking for...
          inputStream.Get() != *it) // ...or did we find something different?
      {
          std::string str;
          size_t len = sExpected.length();
          if (len > 0)
          {
              char* w = new char[len + 1];
              size_t s = wcstombs(w, sExpected.c_str(), len);
              if (s != len)
                  w[0] = 0;
              else
                  w[s] = 0;
              str = std::string(w);
              delete[] w;
          }
          std::string sMessage = "Expected string: " + str;
         throw ScanException(sMessage, inputStream.GetLocation());
      }
   }

   // all's well if we made it here, return quietly
}


inline void Reader::MatchString(std::wstring& string, InputStream& inputStream)
{
   MatchExpectedString(L"\"", inputStream);
   
   while (inputStream.EOS() == false &&
          inputStream.Peek() != '"')
   {
      wchar_t c = inputStream.Get();

      // escape?
      if (c == L'\\' &&
          inputStream.EOS() == false) // shouldn't have reached the end yet
      {
         c = inputStream.Get();
         switch (c) {
            case L'/':      string.push_back(L'/');     break;
            case L'"':      string.push_back(L'"');     break;
            case L'\\':     string.push_back(L'\\');    break;
            case L'b':      string.push_back(L'\b');    break;
            case L'f':      string.push_back(L'\f');    break;
            case L'n':      string.push_back(L'\n');    break;
            case L'r':      string.push_back(L'\r');    break;
            case L't':      string.push_back(L'\t');    break;
            case L'u':      // TODO: what do we do with this?
            default: {
                char w;
                wcstombs(&w, &c, 1);
                std::string sMessage = std::string("Unrecognized escape sequence found in string: \\") + w;
                throw ScanException(sMessage, inputStream.GetLocation());
            }
         }
      }
      else {
         string.push_back(c);
      }
   }

   // eat the last '"' that we just peeked
   MatchExpectedString(L"\"", inputStream);
}


inline void Reader::MatchNumber(std::wstring& sNumber, InputStream& inputStream)
{
   const wchar_t sNumericChars[] = L"0123456789.eE-+";
   std::set<wchar_t> numericChars;
   numericChars.insert(sNumericChars, sNumericChars + sizeof(sNumericChars));

   while (inputStream.EOS() == false &&
          numericChars.find(inputStream.Peek()) != numericChars.end())
   {
      sNumber.push_back(inputStream.Get());   
   }
}


inline void Reader::Parse(UnknownElement& element, Reader::TokenStream& tokenStream) 
{
   if (tokenStream.EOS()) {
      std::string sMessage = "Unexpected end of token stream";
      throw ParseException(sMessage, Location(), Location()); // nowhere to point to
   }

   const Token& token = tokenStream.Peek();
   switch (token.nType) {
      case Token::TOKEN_OBJECT_BEGIN:
      {
         // implicit non-const cast will perform conversion for us (if necessary)
         Object& object = element;
         Parse(object, tokenStream);
         break;
      }

      case Token::TOKEN_ARRAY_BEGIN:
      {
         Array& array = element;
         Parse(array, tokenStream);
         break;
      }

      case Token::TOKEN_STRING:
      {
         String& string = element;
         Parse(string, tokenStream);
         break;
      }

      case Token::TOKEN_NUMBER:
      {
         Number& number = element;
         Parse(number, tokenStream);
         break;
      }

      case Token::TOKEN_BOOLEAN:
      {
         Boolean& boolean = element;
         Parse(boolean, tokenStream);
         break;
      }

      case Token::TOKEN_NULL:
      {
         Null& null = element;
         Parse(null, tokenStream);
         break;
      }

      default:
      {
          std::string str;
          size_t len = token.sValue.length();
          if (len > 0)
          {
              char* w = new char[len + 1];
              size_t s = wcstombs(w, token.sValue.c_str(), len);
              if (s != len)
                  w[0] = 0;
              else
                  w[s] = 0;
              str = std::string(w);
              delete[] w;
          }
          std::string sMessage = "Unexpected token: " + str;
          throw ParseException(sMessage, token.locBegin, token.locEnd);
      }
   }
}


inline void Reader::Parse(Object& object, Reader::TokenStream& tokenStream)
{
   MatchExpectedToken(Token::TOKEN_OBJECT_BEGIN, tokenStream);

   bool bContinue = (tokenStream.EOS() == false &&
                     tokenStream.Peek().nType != Token::TOKEN_OBJECT_END);
   while (bContinue)
   {
      Object::Member member;

      // first the member name. save the token in case we have to throw an exception
      const Token& tokenName = tokenStream.Peek();
      member.name = MatchExpectedToken(Token::TOKEN_STRING, tokenStream);

      // ...then the key/value separator...
      MatchExpectedToken(Token::TOKEN_MEMBER_ASSIGN, tokenStream);

      // ...then the value itself (can be anything).
      Parse(member.element, tokenStream);

      // try adding it to the object (this could throw)
      try
      {
         object.Insert(member);
      }
      catch (Exception&)
      {
         // must be a duplicate name
          std::string str;
          size_t len = member.name.length();
          if (len > 0)
          {
              char* w = new char[len + 1];
              size_t s = wcstombs(w, member.name.c_str(), len);
              if (s != len)
                  w[0] = 0;
              else
                  w[s] = 0;
              str = std::string(w);
              delete[] w;
          }
          std::string sMessage = "Duplicate object member token: " + str;
          throw ParseException(sMessage, tokenName.locBegin, tokenName.locEnd);
      }

      bContinue = (tokenStream.EOS() == false &&
                   tokenStream.Peek().nType == Token::TOKEN_NEXT_ELEMENT);
      if (bContinue)
         MatchExpectedToken(Token::TOKEN_NEXT_ELEMENT, tokenStream);
   }

   MatchExpectedToken(Token::TOKEN_OBJECT_END, tokenStream);
}


inline void Reader::Parse(Array& array, Reader::TokenStream& tokenStream)
{
   MatchExpectedToken(Token::TOKEN_ARRAY_BEGIN, tokenStream);

   bool bContinue = (tokenStream.EOS() == false &&
                     tokenStream.Peek().nType != Token::TOKEN_ARRAY_END);
   while (bContinue)
   {
      // ...what's next? could be anything
      Array::iterator itElement = array.Insert(UnknownElement());
      UnknownElement& element = *itElement;
      Parse(element, tokenStream);

      bContinue = (tokenStream.EOS() == false &&
                   tokenStream.Peek().nType == Token::TOKEN_NEXT_ELEMENT);
      if (bContinue)
         MatchExpectedToken(Token::TOKEN_NEXT_ELEMENT, tokenStream);
   }

   MatchExpectedToken(Token::TOKEN_ARRAY_END, tokenStream);
}


inline void Reader::Parse(String& string, Reader::TokenStream& tokenStream)
{
   string = MatchExpectedToken(Token::TOKEN_STRING, tokenStream);
}


inline void Reader::Parse(Number& number, Reader::TokenStream& tokenStream)
{
   const Token& currentToken = tokenStream.Peek(); // might need this later for throwing exception
   const std::wstring& sValue = MatchExpectedToken(Token::TOKEN_NUMBER, tokenStream);

   std::wistringstream iStr(sValue);
   double dValue;
   iStr >> dValue;

   // did we consume all characters in the token?
   if (iStr.eof() == false)
   {
       wchar_t wc = iStr.peek();
       char w;
       wcstombs(&w, &wc, 1);
       std::string sMessage = std::string("Unexpected character in NUMBER token: ") + w;
       throw ParseException(sMessage, currentToken.locBegin, currentToken.locEnd);
   }

   number = dValue;
}


inline void Reader::Parse(Boolean& boolean, Reader::TokenStream& tokenStream)
{
   const std::wstring& sValue = MatchExpectedToken(Token::TOKEN_BOOLEAN, tokenStream);
   boolean = (sValue == L"true" ? true : false);
}


inline void Reader::Parse(Null&, Reader::TokenStream& tokenStream)
{
   MatchExpectedToken(Token::TOKEN_NULL, tokenStream);
}


inline const std::wstring& Reader::MatchExpectedToken(Token::Type nExpected, Reader::TokenStream& tokenStream)
{
   if (tokenStream.EOS())
   {
      std::string sMessage = "Unexpected End of token stream";
      throw ParseException(sMessage, Location(), Location()); // nowhere to point to
   }

   const Token& token = tokenStream.Get();
   if (token.nType != nExpected)
   {
       std::string str;
       size_t len = token.sValue.length();
       if (len > 0)
       {
           char* w = new char[len + 1];
           size_t s = wcstombs(w, token.sValue.c_str(), len);
           if (s != len)
               w[0] = 0;
           else
               w[s] = 0;
           str = std::string(w);
           delete[] w;
       }
       std::string sMessage = "Unexpected token: " + str;
       throw ParseException(sMessage, token.locBegin, token.locEnd);
   }

   return token.sValue;
}

} // End namespace
