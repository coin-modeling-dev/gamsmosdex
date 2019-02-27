#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <iostream>

#include <vector>
#include <map>
#include <set>
#include <string>

#define RAPIDJSON_HAS_STDSTRING 1
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

#include "gmomcc.h"
#include "gevmcc.h"
#include "dctmcc.h"

class Entity
{
public:
   std::string name;
   int dim;

   int symIdx;

   enum {
      Variable,
      Equation
   } type;
};

class Domain
{
public:
   std::string name;
   std::set<int> uelIdxs;
};


// a block of coefficients
class Coefficient
{
public:
   int rowSymIdx;
   int colSymIdx;

   // for each column domain indicates the row domain index it equals to, or -1 if none
   int colDomEqualsRowDom[GMS_MAX_INDEX_DIM];

   Coefficient(int rowSymIdx_, int colSymIdx_)
   : rowSymIdx(rowSymIdx_), colSymIdx(colSymIdx_)
   {
      for( int i = 0; i < GMS_MAX_INDEX_DIM; ++i )
         colDomEqualsRowDom[i] = -1;
   }

   void analyzeDomains(dctHandle_t dct)
   {
      int rowDomIdx[GMS_MAX_INDEX_DIM];
      int rowDim;
      int colDomIdx[GMS_MAX_INDEX_DIM];
      int colDim;

      dctSymDomIdx(dct, rowSymIdx, rowDomIdx, &rowDim);
      dctSymDomIdx(dct, colSymIdx, colDomIdx, &colDim);

      // performance of this might be slow
      for( int c = 0; c < colDim; ++c )
      {
         for( int r = 0; r < rowDim && colDomEqualsRowDom[c] < 0; ++r )
         {
            if( rowDomIdx[r] != colDomIdx[c] )
               continue;

            // std::cout << "compare rowsymbol " << rowSymIdx << " colsymbol " << colSymIdx << " coldim " << c << " rowdim " << r << std::endl;

            bool uelsequal = true;
            for( auto& e : entries )
            {
               int rowUels[GMS_MAX_INDEX_DIM];
               int colUels[GMS_MAX_INDEX_DIM];
               int symindex;
               int dim;

               dctRowUels(dct, std::get<0>(e), &symindex, rowUels, &dim);
               dctColUels(dct, std::get<1>(e), &symindex, colUels, &dim);

               char uelLabel[GMS_SSSIZE];
               uelLabel[0] = '\0';
               dctUelLabel(dct, rowUels[r], uelLabel, uelLabel, sizeof(uelLabel));
               // std::cout << "  row: " << rowUels[r] << ' ' << uelLabel;
               uelLabel[0] = '\0';
               dctUelLabel(dct, colUels[c], uelLabel, uelLabel, sizeof(uelLabel));
               // std::cout << "  col: " << colUels[c] << ' ' << uelLabel << std::endl;

               if( rowUels[r] != colUels[c] )
               {
                  uelsequal = false;
                  break;
               }
            }

            if( uelsequal )
               colDomEqualsRowDom[c] = r;
         }
      }
   }

   // equation index, variable index, coefficient
   std::vector<std::tuple<int, int, double> > entries;

};

std::vector<Entity> entities;
// equation symbol index, variable symbol index
std::map<std::pair<int, int>, Coefficient> coefs;

static
void analyzeDict(
   dctHandle_t dct
   )
{
   char msg[GMS_SSSIZE];
   dctGetReady(msg, sizeof(msg));

   int nsyms = dctNLSyms(dct);

   for( int i = 0; i < nsyms; ++i )
   {
      char symName[GMS_SSSIZE];
      char symText[GMS_SSSIZE];
      dcttypes symType;
      int symDim;
      int symDomIdx[GMS_MAX_INDEX_DIM];

      dctSymName(dct, i, symName, sizeof(symName));
      symType = (dcttypes)dctSymType(dct, i);
      symDim = dctSymDim(dct, i);

      symText[0] = '\0';
      dctSymText(dct, i, symText, symText, sizeof(symText));

      dctSymDomIdx(dct, i, symDomIdx, &symDim);
      std::cout << "Symbol " << i << " = " << symName << '(';
      for( int d = 0; d < symDim; ++d )
      {
         char domName[GMS_SSSIZE];
         dctDomName(dct, symDomIdx[d], domName, sizeof(domName));
         if( d > 0 )
            std::cout << ",";
         std::cout << domName;
      }
      std::cout << ") type " << symType << " dim " << symDim << " (" << symText << ")" << std::endl;

      if( symType == dctvarSymType || symType == dcteqnSymType )
      {
         Entity e;
         e.name = symName;
         e.dim = symDim;
         e.symIdx = i;
         e.type = symType == dctvarSymType ? Entity::Variable : Entity::Equation;

         entities.push_back(e);
      }
   }

#if 0
   for( int u = 0; u < dctNUels(dct); ++u )
   {
      char uelLabel[GMS_SSSIZE];
      uelLabel[0] = '\0';
      dctUelLabel(dct, u, uelLabel, uelLabel, sizeof(uelLabel));
      std::cout << "UEL " << u << " = " << uelLabel << std::endl;
   }
#endif

}

// declare index for each variable and equation
void printInputDataModel(
   rapidjson::PrettyWriter<rapidjson::StringBuffer>& w,
   dctHandle_t dct
   )
{
   std::cout << std::endl;
   std::cout << "INPUT_DATA_MODEL" << std::endl;
   w.Key("INPUT_DATA_MODEL");
   w.StartObject();

   int domIdx[GMS_MAX_INDEX_DIM];

   for( auto& e : entities )
   {
      if( e.dim == 0 )
         continue;

      std::string indexname = e.name + "_index";
      std::cout << indexname << ":" << std::endl;
      w.Key(indexname);

      int dim;
      dctSymDomIdx(dct, e.symIdx, domIdx, &dim);

      w.StartObject();
      for( int d = 0; d < e.dim; ++d )
      {
         char domName[GMS_SSSIZE];
         dctDomName(dct, domIdx[d], domName, sizeof(domName));

         std::string key = std::string("*") + domName + '#' + e.name;
         std::cout << "  " << key << ": String" << std::endl;
         w.Key(key);
         w.String("String");
      }
      w.EndObject();
   }

   for( auto& cit : coefs )
   {
      Coefficient& c(cit.second);

      char equName[GMS_SSSIZE];
      char varName[GMS_SSSIZE];

      dctSymName(dct, c.rowSymIdx, equName, sizeof(equName));
      dctSymName(dct, c.colSymIdx, varName, sizeof(varName));

      std::string indexname = std::string("coef_") + equName + "_" + varName;
      std::cout << indexname << ":" << std::endl;
      w.Key(indexname);

      int rowDomIdx[GMS_MAX_INDEX_DIM];
      int rowDim;
      int colDomIdx[GMS_MAX_INDEX_DIM];
      int colDim;

      dctSymDomIdx(dct, c.rowSymIdx, rowDomIdx, &rowDim);
      dctSymDomIdx(dct, c.colSymIdx, colDomIdx, &colDim);

      char domName[GMS_SSSIZE];

      w.StartObject();
      for( int r = 0; r < rowDim; ++r )
      {
         dctDomName(dct, rowDomIdx[r], domName, sizeof(domName));
         std::string key = std::string(domName) + "#" + equName;
         std::cout << "  " << key << ": String" << std::endl;
         w.Key(key);
         w.String("String");
      }

      for( int cd = 0; cd < colDim; ++cd )
      {
         char domName[GMS_SSSIZE];
         dctDomName(dct, colDomIdx[cd], domName, sizeof(domName));

         std::cout << "  ";
         if( c.colDomEqualsRowDom[cd] >= 0 )
             std::cout << "(";
         std::string key = std::string(domName) + '#' + varName;
         std::cout << key << ": String";
         if( c.colDomEqualsRowDom[cd] >= 0 )
            std::cout << ")";
         std::cout << std::endl;

         if( c.colDomEqualsRowDom[cd] < 0 )
         {
            w.Key(key);
            w.String("String");
         }
      }

      std::cout << "  val: Double" << std::endl;
      w.Key("val");
      w.String("Double");

      w.EndObject();
   }

   w.EndObject();
}

// print index entries for each variable and equation
static
void printIndexData(
   rapidjson::PrettyWriter<rapidjson::StringBuffer>& w,
   dctHandle_t dct
   )
{
   int domIdx[GMS_MAX_INDEX_DIM];
   int uelIndices[GMS_MAX_INDEX_DIM];
   char domName[GMS_SSSIZE];
   char uelLabel[GMS_SSSIZE];

   for( auto& e : entities )
   {
      if( e.dim == 0 )
         continue;

      int dim;
      dctSymDomIdx(dct, e.symIdx, domIdx, &dim);

      std::string indexname = e.name + "_index";
      std::cout << indexname << ":" << std::endl;
      w.Key(indexname);

      w.StartArray();

      for( int idx = dctSymOffset(dct, e.symIdx); ; ++idx )
      {
         int symIndex;
         int symDim;
         assert(idx >= 0);

         if( e.type == Entity::Variable )
         {
            if( idx >= dctNCols(dct) )
               break;
            dctColUels(dct, idx, &symIndex, uelIndices, &symDim);
         }
         else
         {
            if( idx >= dctNRows(dct) )
               break;
            dctRowUels(dct, idx, &symIndex, uelIndices, &symDim);
         }
         if(symIndex != e.symIdx)
            break;

         assert(symDim == e.dim);

         w.StartObject();
         for( int d = 0; d < e.dim; ++d )
         {
            dctDomName(dct, domIdx[d], domName, sizeof(domName));

            uelLabel[0] = '\0';
            dctUelLabel(dct, uelIndices[d], uelLabel, uelLabel, sizeof(uelLabel));

            std::string key = std::string(domName) + '#' + e.name;
            std::cout << "  " << key << ":" << uelLabel;

            w.Key(key);
            w.String(uelLabel);
         }
         std::cout << std::endl;
         w.EndObject();
      }

      w.EndArray();
   }
}

void analyzeMatrix(
   gmoHandle_t gmo,
   dctHandle_t dct)
{
   double jacval;
   int colidx;
   int nlflag;

   int rowSymIdx;
   int rowUels[GMS_MAX_INDEX_DIM];
   int rowSymDim;

   int colSymIdx;
   int colUels[GMS_MAX_INDEX_DIM];
   int colSymDim;

   for( int rowidx = 0; rowidx < gmoM(gmo); ++rowidx )
   {
      dctRowUels(dct, gmoGetiModel(gmo, rowidx), &rowSymIdx, rowUels, &rowSymDim);

      void* jacptr = NULL;
      gmoGetRowJacInfoOne(gmo, rowidx, &jacptr, &jacval, &colidx, &nlflag);
      while( jacptr != NULL )
      {
         dctColUels(dct, gmoGetiModel(gmo, colidx), &colSymIdx, colUels, &colSymDim);

         if( coefs.count(std::pair<int,int>(rowSymIdx, colSymIdx)) == 0 )
         {
            coefs.insert(std::pair<std::pair<int,int>, Coefficient>(std::pair<int,int>(rowSymIdx, colSymIdx), Coefficient(rowSymIdx, colSymIdx)));
         }

         Coefficient& c(coefs.at(std::pair<int,int>(rowSymIdx, colSymIdx)));
         c.entries.push_back(std::tuple<int, int, double>(gmoGetiModel(gmo, rowidx), gmoGetiModel(gmo, colidx), jacval));

         gmoGetRowJacInfoOne(gmo, rowidx, &jacptr, &jacval, &colidx, &nlflag);
      }
   }
}

void printCoefficientData(
   rapidjson::PrettyWriter<rapidjson::StringBuffer>& w,
   dctHandle_t dct
   )
{
   char equName[GMS_SSSIZE];
   char varName[GMS_SSSIZE];

   int rowUels[GMS_MAX_INDEX_DIM];
   int colUels[GMS_MAX_INDEX_DIM];
   int rowDim;
   int colDim;

   char uelLabel[GMS_SSSIZE];

   int rowDomIdx[GMS_MAX_INDEX_DIM];
   int colDomIdx[GMS_MAX_INDEX_DIM];
   char domName[GMS_SSSIZE];

   for( auto& cit : coefs )
   {
      Coefficient& c(cit.second);
      dctSymName(dct, c.rowSymIdx, equName, sizeof(equName));
      dctSymName(dct, c.colSymIdx, varName, sizeof(varName));

      dctSymDomIdx(dct, c.rowSymIdx, rowDomIdx, &rowDim);
      dctSymDomIdx(dct, c.colSymIdx, colDomIdx, &colDim);

      std::string coefname = std::string("coef_") + equName + "_" + varName;
      std::cout << coefname << ":" << std::endl;
      w.Key(coefname);
      w.StartArray();

      for( auto& e : c.entries )
      {
         int symidx;
         dctRowUels(dct, std::get<0>(e), &symidx, rowUels, &rowDim);
         dctColUels(dct, std::get<1>(e), &symidx, colUels, &colDim);

         w.StartObject();
         if( rowDim > 0 )
         {
            for( int d = 0; d < rowDim; ++d )
            {
               dctDomName(dct, rowDomIdx[d], domName, sizeof(domName));

               uelLabel[0] = '\0';
               dctUelLabel(dct, rowUels[d], uelLabel, uelLabel, sizeof(uelLabel));

               std::string key = std::string(domName) + '#' + equName;
               std::cout << "  " << key << ":" << uelLabel;
               w.Key(key);
               w.String(uelLabel);
            }
            std::cout << ", ";
         }

         if( colDim > 0 )
         {
            for( int d = 0; d < colDim; ++d )
            {
               dctDomName(dct, colDomIdx[d], domName, sizeof(domName));

               uelLabel[0] = '\0';
               dctUelLabel(dct, colUels[d], uelLabel, uelLabel, sizeof(uelLabel));

               std::cout << "  ";
               if( c.colDomEqualsRowDom[d] >= 0 )
                  std::cout << '(';
               std::string key  = std::string(domName) + '#' + varName;
               std::cout << key << ":" << uelLabel;
               if( c.colDomEqualsRowDom[d] >= 0 )
                  std::cout << ')';

               if( c.colDomEqualsRowDom[d] < 0 )
               {
                  w.Key(key);
                  w.String(uelLabel);
               }
            }
            std::cout << ", ";
         }

         std::cout << "val:" << std::get<2>(e) << std::endl;
         w.Key("val");
         w.Double(std::get<2>(e));

         w.EndObject();
      }

      w.EndArray();
   }
}

void printEntities(
   rapidjson::PrettyWriter<rapidjson::StringBuffer>& w,
   dctHandle_t dct,
   int type
   )
{
   std::cout << std::endl;
   if( type == Entity::Variable )
   {
      std::cout << "VARIABLES:" << std::endl;
      w.Key("Variables");
   }
   else
   {
      std::cout << "CONSTRAINTS:" << std::endl;
      w.Key("Constraints");
   }

   w.StartArray();
   for( auto& e : entities )
   {
      if( e.type != type )
         continue;

      w.StartObject();

      std::cout << "Name: " << e.name << std::endl;
      w.Key("Name");
      w.String(e.name);

      w.Key("Index");
      if( e.dim > 0 )
      {
         std::string indexname = e.name + "_index";
         std::cout << "Index: " << indexname << std::endl;
         w.String(indexname);
      }
      else
      {
         std::cout << "Index: self" << std::endl;
         w.String("self");
      }
      std::cout << std::endl;
      w.EndObject();
   }
   w.EndArray();
}

void printCoefficients(
   rapidjson::PrettyWriter<rapidjson::StringBuffer>& w,
   dctHandle_t dct
   )
{
   std::cout << std::endl;
   std::cout << "COEFFICIENTS:" << std::endl;
   w.Key("Coefficients");

   char equName[GMS_SSSIZE];
   char varName[GMS_SSSIZE];

   w.StartArray();
   for( auto& cit : coefs )
   {
      Coefficient& c(cit.second);

      dctSymName(dct, c.rowSymIdx, equName, sizeof(equName));
      dctSymName(dct, c.colSymIdx, varName, sizeof(varName));

      w.StartObject();

      std::cout << "Constraints: " << equName << std::endl;
      w.Key("Constraints");
      w.String(equName);

      std::cout << "Variables: " << varName << std::endl;
      w.Key("Variables");
      w.String("varName");

      std::string entry = std::string("coef_") + equName + '_' + varName + ".val";
      std::cout << "Entries: coef_" << equName << "_" << varName << ".val" << std::endl;
      w.Key("Entries");
      w.String(entry);

      int colDomIdx[GMS_MAX_INDEX_DIM];
      int colDim;
      dctSymDomIdx(dct, c.colSymIdx, colDomIdx, &colDim);

      std::string cond;
      for( int d = 0; d < colDim; ++d )
      {
         if( c.colDomEqualsRowDom[d] >= 0 )
         {
            char domName[GMS_SSSIZE];
            dctDomName(dct, colDomIdx[d], domName, sizeof(domName));
            if( cond != "" )
               cond += " and ";
            cond += std::string(varName) + "." + domName + '#' + varName + " == ";
            cond += std::string(equName) + "." + domName + '#' + equName;
         }
      }
      std::cout << "Condition: " << cond << std::endl;

      w.Key("Condition");
      w.String(cond);

      std::cout << std::endl;
      w.EndObject();
   }
   w.EndArray();
}


int main(
   int    argc,
   char** argv
)
{
   char buffer[GMS_SSSIZE];
   gmoHandle_t gmo;
   gevHandle_t gev;
   dctHandle_t dct;
   int rc = EXIT_FAILURE;

   if( argc < 2 )
   {
      printf("usage: %s <cntrlfile>\n", argv[0]);
      return 1;
   }

   /* create GMO and GEV handles */
   if( !gmoCreate(&gmo, buffer, sizeof(buffer)) || !gevCreate(&gev, buffer, sizeof(buffer)) )
   {
      fprintf(stderr, "%s\n", buffer);
      goto TERMINATE;
   }

   /* load GAMS control file */
   if( gevInitEnvironmentLegacy(gev, argv[1]) )
   {
      fprintf(stderr, "Could not load control file %s\n", argv[1]);
      goto TERMINATE;
   }

   /* let gmo know about gev */
   if( gmoRegisterEnvironment(gmo, gev, buffer) )
   {
      fprintf(stderr, "Error registering GAMS Environment: %s\n", buffer);
      goto TERMINATE;
   }

   /* load instance data */
   if( gmoLoadDataLegacy(gmo, buffer) )
   {
      fprintf(stderr, "Could not load model data.\n");
      goto TERMINATE;
   }

   gevTerminateUninstall(gev);

   dct = (dctHandle_t)gmoDict(gmo);
   assert(dct != NULL);

   analyzeDict(dct);
   analyzeMatrix(gmo, dct);
   for( auto& c : coefs )
      c.second.analyzeDomains(dct);

   {

   rapidjson::StringBuffer s;
   rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(s);
   writer.StartObject();

   printInputDataModel(writer, dct);

   std::cout << std::endl;
   std::cout << "DATA" << std::endl;
   writer.Key("DATA");
   writer.StartObject();
   printIndexData(writer, dct);
   printCoefficientData(writer, dct);
   writer.EndObject();

   printEntities(writer, dct, Entity::Variable);
   printEntities(writer, dct, Entity::Equation);
   printCoefficients(writer, dct);

   writer.EndObject();
   std::cout << s.GetString() << std::endl;

   }


   rc = EXIT_SUCCESS;

TERMINATE:

   if( gmo != NULL )
   {
      gmoUnloadSolutionLegacy(gmo);
      gmoFree(&gmo);
   }
   if( gev != NULL )
      gevFree(&gev);

   gmoLibraryUnload();
   gevLibraryUnload();
   dctLibraryUnload();

   return rc;
}
