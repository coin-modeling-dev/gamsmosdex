#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <iostream>

#include <vector>
#include <set>
#include <string>

#define RAPIDJSON_HAS_STDSTRING 1
#include "rapidjson/filereadstream.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

using namespace rapidjson;

int processInputDataModel(
   std::ostream&  out,
   Document&      d
   )
{
   assert(d.IsObject());
   assert(d.HasMember("INPUT_DATA_MODEL"));

   auto& inputdata = d["INPUT_DATA_MODEL"];
   assert(inputdata.IsObject());

   // first collect all keynames -> they should become set declarations
   // and all other column names
   std::set<std::string> keynames;
   std::set<std::string> othernames;
   for( Value::ConstMemberIterator itr = inputdata.MemberBegin(); itr != inputdata.MemberEnd(); ++itr )
   {
       //out << "Type of member " << itr->name.GetString() << " is " << itr->value.GetType() << std::endl;
      assert(itr->value.IsObject());
      for( Value::ConstMemberIterator itr2 = itr->value.MemberBegin(); itr2 != itr->value.MemberEnd(); ++itr2 )
      {
         // keynames are identfied by leading '*'
         if( *itr2->name.GetString() == '*' )
         {
            assert(itr2->value.IsString());
            assert(itr2->value == "String");

            keynames.insert(std::string(itr2->name.GetString() + 1));
         }
         else
         {
            assert(itr2->value.IsString());
            assert(itr2->value == "Double");

            othernames.insert(itr2->name.GetString());
         }
      }
   }

   // declare sets
   for( const std::string& key : keynames )
      out << "Set " << key << ";" << std::endl;

   // declare further set for other column names
   out << "Set cols / ";
   bool first = true;
   for( const std::string& col : othernames )
   {
      if( !first )
         out << ", ";
      out << col;
      first = false;
   }
   out << " /;" << std::endl;

   // declare parameters and dynamic sets
   for( Value::ConstMemberIterator itr = inputdata.MemberBegin(); itr != inputdata.MemberEnd(); ++itr )
   {
      assert(itr->value.IsObject());

      std::string param(itr->name.GetString());
      param += "(";

      bool hasdata = false;
      bool first = true;
      for( Value::ConstMemberIterator itr2 = itr->value.MemberBegin(); itr2 != itr->value.MemberEnd(); ++itr2 )
      {
         // keynames are identfied by leading '*'
         if( *itr2->name.GetString() == '*' )
         {
            assert(itr2->value.IsString());
            assert(itr2->value == "String");

            if( !first )
               param += ", ";
            else
               first = false;

            param += std::string(itr2->name.GetString() + 1);
         }
         else
         {
            assert(itr2->value.IsString());
            assert(itr2->value == "Double");

            hasdata = true;
         }
      }
      if( hasdata )
      {
         param += ", cols);";
         out << "Parameter " << param << std::endl;
      }
      else
      {
         param += ");";
         out << "Set " << param << std::endl;
      }
   }

   return 0;
}

int processData(
   std::ostream&  out,
   Document&      d
   )
{
   assert(d.IsObject());
   assert(d.HasMember("DATA"));

   auto& data = d["DATA"];
   assert(data.IsObject());

   for( Value::ConstMemberIterator itr = data.MemberBegin(); itr != data.MemberEnd(); ++itr )
   {
      auto& decl = d["INPUT_DATA_MODEL"][itr->name];

      std::string param(itr->name.GetString());
      param += "(";

      std::vector<std::string> keys;
      std::set<std::string> other;

      bool first = true;
      for( Value::ConstMemberIterator itr2 = decl.MemberBegin(); itr2 != decl.MemberEnd(); ++itr2 )
      {
         // keynames are identfied by leading '*'
         if( *itr2->name.GetString() == '*' )
         {
            assert(itr2->value.IsString());
            assert(itr2->value == "String");

            if( !first )
               param += ", ";
            else
               first = false;

            param += std::string(itr2->name.GetString() + 1) + '<';

            keys.push_back(itr2->name.GetString() + 1);
         }
         else
         {
            assert(itr2->value.IsString());
            assert(itr2->value == "Double");

            other.insert(itr2->name.GetString());
         }
      }
      if( !other.empty() )
      {
         param += ", cols) /";
         out << "Parameter " << param << std::endl;
      }
      else
      {
         param += ") /";
         out << "Set " << param << std::endl;
      }

      assert(itr->value.IsArray());
      for( Value::ConstValueIterator itr2 = itr->value.Begin(); itr2 != itr->value.End(); ++itr2 )
      {
         assert(itr2->IsObject());
         std::string keystring;
         bool first = true;
         for( auto& key : keys )
         {
            assert(itr2->HasMember(key));

            if( !first )
               keystring += '.';
            else
               first = false;

            keystring += '\'';
            keystring += (*itr2)[key].GetString();
            keystring += '\'';
         }

         if( other.empty() )
         {
            out << "  " << keystring << std::endl;
         }
         else
         {
            for( auto& o : other )
            {
               if( itr2->HasMember(o) )
               {
                  out << "  " << keystring << ".'" << o << "' " << (*itr2)[o].GetDouble() << std::endl;
               }
            }
         }
      }
      out << "/;" << std::endl;
   }

   return 0;
}

int processVariables(
   std::ostream&  out,
   Document&      d
   )
{
   assert(d.IsObject());
   assert(d.HasMember("VARIABLES"));

   auto& vars = d["VARIABLES"];
   assert(vars.IsArray());

   for( Value::ConstValueIterator itr = vars.Begin(); itr != vars.End(); ++itr )
   {
      auto& var = *itr;
      assert(var.IsObject());

      std::string vardomstr;

      auto& decl = d["INPUT_DATA_MODEL"][var["INDEX"]];

      std::vector<std::string> keys;
      std::set<std::string> other;

      bool first = true;
      for( Value::ConstMemberIterator itr2 = decl.MemberBegin(); itr2 != decl.MemberEnd(); ++itr2 )
      {
         // keynames are identfied by leading '*'
         if( *itr2->name.GetString() == '*' )
         {
            assert(itr2->value.IsString());
            assert(itr2->value == "String");

            if( !first )
               vardomstr += ", ";
            else
               first = false;

            vardomstr += std::string(itr2->name.GetString() + 1);
         }
      }
      if( var["TYPE"] == "INTEGER")
         out << "Integer ";   // FIXME this implies a lower bound of 0
      else if( var["TYPE"] == "BINARY")
         out << "Binary ";
      out << "Variable " << var["NAME"].GetString() << '(' + vardomstr << ");" << std::endl;

      if( var.HasMember("BOUNDS") )
      {
         auto& bounds = var["BOUNDS"];
         if( bounds.HasMember("LOWER") )
         {
            out << var["NAME"].GetString() << ".lo(" << vardomstr << ") = ";
            auto& lb = bounds["LOWER"];
            assert(lb.IsDouble() || lb.IsString());  // integer?
            if( lb.IsDouble() )
            {
                out << lb.GetDouble();
            }
            else try
            {
               out << std::stod(lb.GetString());
            }
            catch( const std::invalid_argument& )
            {
               // FIXME we now just assume that lb starts with the variable name and we only care about that comes after
               std::string lbstr = lb.GetString();
               std::string::size_type pos = lbstr.find(".");
               assert(pos != std::string::npos);
               out << var["INDEX"].GetString() << "(" << vardomstr << ", '" << std::string(lbstr, pos+1) << "')";
            }
            out  << ";" << std::endl;
         }

         if( bounds.HasMember("UPPER") )
         {
            out << var["NAME"].GetString() << ".uo(" << vardomstr << ") = ";
            auto& ub = bounds["UPPER"];
            assert(ub.IsDouble() || ub.IsString());  // integer?
            if( ub.IsDouble() )
            {
                out << ub.GetDouble();
            }
            else try
            {
               out << std::stod(ub.GetString());
            }
            catch( const std::invalid_argument& )
            {
               // FIXME see above for lb
               std::string ubstr = ub.GetString();
               std::string::size_type pos = ubstr.find(".");
               assert(pos != std::string::npos);
               out << var["INDEX"].GetString() << "(" << vardomstr << ", '" << std::string(ubstr, pos+1) << "')";
            }
            out  << ";" << std::endl;
         }
      }
   }

   return 0;
}

int main(
   int    argc,
   char** argv
)
{
   if( argc <= 1 )
   {
      std::cerr << "Usage: " << argv[0] << " <file.mosdex>" << std::endl;
      return EXIT_FAILURE;
   }

   FILE *fp = fopen(argv[1], "r");
   if( fp == NULL )
   {
       std::cerr << "File " << argv[1] << " not found" << std::endl;
       return EXIT_FAILURE;
   }

   char readBuffer[65536];
   FileReadStream is(fp, readBuffer, sizeof(readBuffer));

   Document d;
   if( d.ParseStream(is).HasParseError() )
   {
      std::cerr << "Error(offset " << d.GetErrorOffset() << "): " << GetParseError_En(d.GetParseError()) << std::endl;
   }

   fclose(fp);

   processInputDataModel(std::cout, d);
   processData(std::cout, d);
   processVariables(std::cout, d);

   return EXIT_SUCCESS;
}