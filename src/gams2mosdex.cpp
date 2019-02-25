#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <iostream>

#include <vector>
#include <map>
#include <set>
#include <string>

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

std::vector<Entity> entities;

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
   dctHandle_t dct
   )
{
   std::cout << std::endl;
   std::cout << "INPUT_DATA_MODEL" << std::endl;

   int domIdx[GMS_MAX_INDEX_DIM];

   for( auto e : entities )
   {
      std::cout << e.name << "_index:" << std::endl;

      if( e.dim == 0 )
         continue;

      int dim;
      dctSymDomIdx(dct, e.symIdx, domIdx, &dim);

      for( int d = 0; d < e.dim; ++d )
      {
         char domName[GMS_SSSIZE];
         dctDomName(dct, domIdx[d], domName, sizeof(domName));
         std::cout << "  *" << domName << ": String" << std::endl;
      }
   }
}

// print index entries for each variable and equation
static
void printIndexData(
   dctHandle_t dct
   )
{
   std::cout << std::endl;
   std::cout << "DATA" << std::endl;

   int uelIndices[GMS_MAX_INDEX_DIM];
   char uelLabel[GMS_SSSIZE];

   for( auto e : entities )
   {
      std::cout << e.name << "_index:" << std::endl;

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

         for( int d = 0; d < e.dim; ++d )
         {
            uelLabel[0] = '\0';
            dctUelLabel(dct, uelIndices[d], uelLabel, uelLabel, sizeof(uelLabel));
            std::cout << "  " << uelLabel;
         }
         std::cout << std::endl;
      }
   }

}

// a block of coefficients
class Coefficient
{
public:
   int rowSymIdx;
   int colSymIdx;

   Coefficient(int rowSymIdx_, int colSymIdx_)
   : rowSymIdx(rowSymIdx_), colSymIdx(colSymIdx_)
   { }

   // equation index, variable index, coefficient
   std::vector<std::tuple<int, int, double> > entries;

};

// equation symbol index, variable symbol index
std::map<std::pair<int, int>, Coefficient> coefs;

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

   for( auto cit : coefs )
   {
      Coefficient& c(cit.second);
      dctSymName(dct, c.rowSymIdx, equName, sizeof(equName));
      dctSymName(dct, c.colSymIdx, varName, sizeof(varName));

      std::cout << "coef_" << equName << "_" << varName << ":" << std::endl;
      for( auto e : c.entries )
      {
         int symidx;
         dctRowUels(dct, std::get<0>(e), &symidx, rowUels, &rowDim);
         dctColUels(dct, std::get<1>(e), &symidx, colUels, &colDim);

         if( rowDim > 0 )
         {
            for( int d = 0; d < rowDim; ++d )
            {
               uelLabel[0] = '\0';
               dctUelLabel(dct, rowUels[d], uelLabel, uelLabel, sizeof(uelLabel));
               std::cout << "  " << uelLabel;
            }
            std::cout << ", ";
         }

         if( colDim > 0 )
         {
            for( int d = 0; d < colDim; ++d )
            {
               uelLabel[0] = '\0';
               dctUelLabel(dct, colUels[d], uelLabel, uelLabel, sizeof(uelLabel));
               std::cout << "  " << uelLabel;
            }
            std::cout << ", ";
         }

         std::cout << std::get<2>(e) << std::endl;
      }
   }
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

   printInputDataModel(dct);
   printIndexData(dct);
   printCoefficientData(dct);

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