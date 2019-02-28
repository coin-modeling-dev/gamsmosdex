#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <iostream>

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
       //std::cout << "Type of member " << itr->name.GetString() << " is " << itr->value.GetType() << std::endl;
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
      std::cout << "Set " << key << ";" << std::endl;

   // declare further set for other column names
   std::cout << "Set cols / ";
   bool first = true;
   for( const std::string& col : othernames )
   {
      if( !first )
         std::cout << ", ";
      std::cout << col;
      first = false;
   }
   std::cout << " /;" << std::endl;

   // declare parameters
   for( Value::ConstMemberIterator itr = inputdata.MemberBegin(); itr != inputdata.MemberEnd(); ++itr )
   {
      assert(itr->value.IsObject());

      std::string param(itr->name.GetString());
      param += "(";

      bool hasdata = false;
      for( Value::ConstMemberIterator itr2 = itr->value.MemberBegin(); itr2 != itr->value.MemberEnd(); ++itr2 )
      {
         // keynames are identfied by leading '*'
         if( *itr2->name.GetString() == '*' )
         {
            assert(itr2->value.IsString());
            assert(itr2->value == "String");

            param += std::string(itr2->name.GetString() + 1);
            param += ", ";
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
         param += "cols)";
         std::cout << "Parameter " << param << std::endl;
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

   return EXIT_SUCCESS;
}