#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <iostream>

#include <vector>
#include <set>
#include <string>
#include <algorithm>

#define RAPIDJSON_HAS_STDSTRING 1
#include "rapidjson/filereadstream.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

using namespace rapidjson;

std::vector<std::string> getDomain(
   Document&   d,
   std::string entity,
   std::string name
)
{
   assert(d.IsObject());

   std::vector<std::string> dom;

   std::string indexname;
   if( entity == "VARIABLE" )
   {
      auto& vars = d["VARIABLES"];
      for( Value::ConstValueIterator itr = vars.Begin(); itr != vars.End(); ++itr )
      {
         if( name == (*itr)["NAME"].GetString() )
         {
            indexname = (*itr)["INDEX"].GetString();
            break;
         }
      }
   }
   else if( entity == "CONSTRAINT" )
   {
      auto& vars = d["CONSTRAINTS"];
      for( Value::ConstValueIterator itr = vars.Begin(); itr != vars.End(); ++itr )
      {
         if( name == (*itr)["NAME"].GetString() )
         {
            indexname = (*itr)["INDEX"].GetString();
            break;
         }
      }
   }
   else if( entity == "INDEX" )
   {
      indexname = name;
   }

   assert(!indexname.empty());

   auto& inputdata = d["INPUT_DATA_MODEL"][indexname];
   for( Value::ConstMemberIterator itr = inputdata.MemberBegin(); itr != inputdata.MemberEnd(); ++itr )
   {
      // keynames are identfied by leading '*'
      if( *itr->name.GetString() == '*' )
      {
         assert(itr->value.IsString());
         assert(itr->value == "String");

         dom.push_back(std::string(itr->name.GetString() + 1));
      }
   }

   return dom;
}

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

#if 0  // cannot declare first if using "<" feature later on
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
#endif

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
            out << var["NAME"].GetString() << ".up(" << vardomstr << ") = ";
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


int processConstraints(
   std::ostream&  out,
   Document&      d
   )
{
   assert(d.IsObject());

   assert(d.HasMember("CONSTRAINTS"));
   auto& cons = d["CONSTRAINTS"];
   assert(cons.IsArray());

   assert(d.HasMember("COEFFICIENTS"));
   auto& coefs = d["COEFFICIENTS"];
   assert(coefs.IsArray());

   for( Value::ConstValueIterator itr = cons.Begin(); itr != cons.End(); ++itr )
   {
      auto& con = *itr;
      assert(con.IsObject());

      std::vector<std::string> condom = getDomain(d, "INDEX", con["INDEX"].GetString());

      std::string condomstr;
      bool first = true;
      for( auto& d : condom )
      {
         if( !first )
            condomstr += ", ";
         else
            first = false;
         condomstr += d;
      }
      //assert(con["TYPE"] == "LINEAR");
      out << "Equation " << con["NAME"].GetString() << '(' + condomstr << ");" << std::endl;

      out << con["NAME"].GetString() << '(' + condomstr << ")..";
      // now assemble terms
      for( Value::ConstValueIterator coefitr = coefs.Begin(); coefitr != coefs.End(); ++coefitr )
      {
         assert(coefitr->IsObject());
         if( !coefitr->HasMember("CONSTRAINTS") )
            continue;
         if( strcmp((*coefitr)["CONSTRAINTS"].GetString(), con["NAME"].GetString()) != 0 )
            continue;

         out << " +";
         out << (*coefitr)["ENTRIES"].GetString() << " * ";  // TODO this needs more processing

         std::string var = (*coefitr)["VARIABLES"].GetString();
         std::vector<std::string> vardom = getDomain(d, "VARIABLE", var);


         // process matching of variable and equation indices
         // FIXME assumes very particular format
         std::string sameasstr;
         if( coefitr->HasMember("CONDITION") )
         {
            std::string cond = (*coefitr)["CONDITION"].GetString();
            size_t seppos = cond.find(" == ");
            assert(seppos != std::string::npos);

            std::string first(cond, 0, seppos);
            std::string second(cond, seppos+4);

            first = std::string(first, first.find(".")+1);
            second = std::string(second, second.find(".")+1);

            sameasstr = std::string("$sameas(") + first + "," + second + ")";
         }

         // check which of the constraints domains appear in variables domains
         std::vector<bool> controlled(vardom.size(), false);
         size_t ncontrolled = 0;
         for( auto& ed : condom )
         {
            ptrdiff_t pos = std::find(vardom.begin(), vardom.end(), ed) - vardom.begin();
            if( pos < (int)vardom.size() )
            {
               controlled[pos] = true;
               ++ncontrolled;
            }
         }

         if( ncontrolled < vardom.size() )
         {
            out << "sum((";
            bool first = true;
            for( size_t i = 0; i < vardom.size(); ++i )
            {
               if( !controlled[i] )
               {
                  if( !first )
                     out << ",";
                  else
                     first = false;
                  out << vardom[i];
               }
            }
            out << ")" << sameasstr << ",";
         }
         out << var << "(";
         bool first = true;
         for( auto& d : vardom )
         {
            if( !first )
               out << ",";
            else
               first = false;
            out << d;
         }
         out << ")";
         if( ncontrolled < vardom.size() )
            out << ")";

      }


      std::string rhs = "0.0";
      std::string sense = "=N=";
      if( con.HasMember("BOUNDS") )
      {
         auto& bounds = con["BOUNDS"];
         std::string lbstr;
         std::string ubstr;
         if( bounds.HasMember("LOWER") )
         {
            auto& lb = bounds["LOWER"];
            assert(lb.IsDouble() || lb.IsString());  // integer?
            if( lb.IsDouble() )
            {
               lbstr = std::to_string(lb.GetDouble());
            }
            else try
            {
               lbstr = std::to_string(std::stod(lb.GetString()));
            }
            catch( const std::invalid_argument& )
            {
               // FIXME we now just assume that lb starts with the variable name and we only care about that comes after
               lbstr = lb.GetString();
               std::string::size_type pos = lbstr.find(".");
               assert(pos != std::string::npos);
               lbstr = std::string(con["INDEX"].GetString()) + "(" + condomstr + ", '" + std::string(lbstr, pos+1) + "')";
            }
         }

         if( bounds.HasMember("UPPER") )
         {
            auto& ub = bounds["UPPER"];
            assert(ub.IsDouble() || ub.IsString());  // integer?
            if( ub.IsDouble() )
            {
               ubstr = std::to_string(ub.GetDouble());
            }
            else try
            {
               ubstr = std::to_string(std::stod(ub.GetString()));
            }
            catch( const std::invalid_argument& )
            {
               // FIXME we now just assume that lb starts with the variable name and we only care about that comes after
               ubstr = ub.GetString();
               std::string::size_type pos = ubstr.find(".");
               assert(pos != std::string::npos);
               ubstr = std::string(con["INDEX"].GetString()) + "(" + condomstr + ", '" + std::string(ubstr, pos+1) + "')";
            }
         }

         if( lbstr == ubstr )
         {
            sense = "=E=";
            rhs = lbstr;
         }
         else if( lbstr.empty() )
         {
            sense = "=L=";
            rhs = ubstr;
         }
         else if( ubstr.empty() )
         {
            sense = "=R=";
            rhs = lbstr;
         }
         else
         {
            assert("RANGED CONSTRAINTS NOT ALLOWED" == NULL);
         }
      }

      out << ' ' << sense << ' ' << rhs << ";" << std::endl;
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
   processConstraints(std::cout, d);

   return EXIT_SUCCESS;
}