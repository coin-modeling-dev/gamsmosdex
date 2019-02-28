#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <iostream>

#define RAPIDJSON_HAS_STDSTRING 1
#include "rapidjson/filereadstream.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

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
   rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));

   rapidjson::Document d;
   if( d.ParseStream(is).HasParseError() )
   {
      std::cerr << "Error(offset " << d.GetErrorOffset() << "): " << rapidjson::GetParseError_En(d.GetParseError()) << std::endl;
   }

   fclose(fp);


   return EXIT_SUCCESS;
}