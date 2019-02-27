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

class Domain
{
public:
   Domain(const char* name_, int domIdx_)
   : name(name_), domIdx(domIdx_)
   { }

   std::string name;

   // index of domain in GAMS dct
   int domIdx;
};

// domains, indexed by Dct domain index
std::vector<Domain> domains;

class Symbol
{
public:
   typedef enum {
      None,
      Variable,
      Constraint,
      Objective
   } Type;

   Symbol(const char* name_, Symbol::Type type_, int symIdx_)
   : name(name_), symIdx(symIdx_), type(type_)
   { }

   std::string name;

   // index of symbol in GAMS dct
   int symIdx;

   std::string text;
   std::vector<Domain*> dom;

   Type type;

   int dim()
   {
      return (int)dom.size();
   }

   std::string getDomName(
      int pos
      )
   {
      return dom.at(pos)->name + '#' + name;
   }
};



// a block of coefficients
class Coefficient
{
public:
   Symbol& equation;
   Symbol& variable;

   // for each column domain indicates the row domain index it equals to, or -1 if none
   int varDomEqualsEquDom[GMS_MAX_INDEX_DIM];

   Coefficient(Symbol& equ, Symbol& var)
   : equation(equ), variable(var)
   {
      for( int i = 0; i < var.dim(); ++i )
         varDomEqualsEquDom[i] = -1;
   }

   void analyzeDomains(dctHandle_t dct)
   {
      std::vector<Domain*>& equDom(equation.dom);
      std::vector<Domain*>& varDom(variable.dom);

      // performance of this might be slow (though at most 20x20)
      for( size_t c = 0; c < equDom.size(); ++c )
      {
         for( size_t r = 0; r < varDom.size() && varDomEqualsEquDom[c] < 0; ++r )
         {
            if( equDom[r] != varDom[c] )
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

#if 0
               char uelLabel[GMS_SSSIZE];
               uelLabel[0] = '\0';
               dctUelLabel(dct, rowUels[r], uelLabel, uelLabel, sizeof(uelLabel));
               // std::cout << "  row: " << rowUels[r] << ' ' << uelLabel;
               uelLabel[0] = '\0';
               dctUelLabel(dct, colUels[c], uelLabel, uelLabel, sizeof(uelLabel));
               // std::cout << "  col: " << colUels[c] << ' ' << uelLabel << std::endl;
#endif

               if( rowUels[r] != colUels[c] )
               {
                  uelsequal = false;
                  break;
               }
            }

            if( uelsequal )
               varDomEqualsEquDom[c] = r;
         }
      }
   }

   std::string getName()
   {
      return std::string("coef_") + equation.name + "_" + variable.name;
   }

   // equation index, variable index, coefficient
   std::vector<std::tuple<int, int, double> > entries;
};

// variables and constraints, indexed by Dct symbol index
std::vector<Symbol> symbols;

// equation symbol index, variable symbol index
std::map<std::pair<int, int>, Coefficient> coefs;

static
void analyzeDict(
   gmoHandle_t gmo,
   dctHandle_t dct
   )
{
   char msg[GMS_SSSIZE];
   dctGetReady(msg, sizeof(msg));

   // add dummy domain because dct domain indexing starts at 1!
   domains.push_back(Domain("dummy", 0));

   int ndoms = dctDomNameCount(dct);
   for( int i = 1; i <= ndoms; ++i )
   {
      char domName[GMS_SSSIZE];
      dctDomName(dct, i, domName, sizeof(domName));
      domains.push_back(Domain(domName, i));
   }

   // make up a symbol for the objective and put it onto position 0 (there is no GAMS symbol at this position)
   char objName[GMS_SSSIZE];
   gmoGetObjName(gmo, objName);
   symbols.push_back(Symbol(objName, Symbol::Objective, 0));

   int nsyms = dctNLSyms(dct);
   for( int i = 1; i <= nsyms; ++i )
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

      Symbol::Type type;
      if( symType == dctvarSymType )
         type = Symbol::Variable;
      else if( symType == dcteqnSymType )
         type = Symbol::Constraint;
      else
         type = Symbol::None;
      symbols.push_back(Symbol(symName, type, i));
      symbols.back().text = symText;

      dctSymDomIdx(dct, i, symDomIdx, &symDim);
      std::cout << "Symbol " << i << " = " << symName << '(';
      for( int d = 0; d < symDim; ++d )
      {
         if( d > 0 )
            std::cout << ",";
         std::cout << domains.at(symDomIdx[d]).name;
         symbols.back().dom.push_back(&domains[symDomIdx[d]]);
      }
      std::cout << ") type " << symType << " dim " << symDim << " (" << symText << ")" << std::endl;

      // check whether symbol is actually used in model
      // if it was the original objective variable or objective constraint, then it could have been reformulated out, though it is still in dct
      // thus, should be enough to do this for 0-dim symbols
      if( symDim == 0 )
      {
         int uelIndices[GMS_MAX_INDEX_DIM];
         memset(uelIndices, 0, sizeof(uelIndices));
         int idx;
         void* ret = dctFindFirstRowCol(dct, i, uelIndices, &idx);
         if( ret == NULL )
            symbols.back().type = Symbol::None;
         else if( symType == dctvarSymType  )
         {
            // FIXME this prints a message if the var is the objective var that has been reformulated out, but then returns -2
            idx = gmoGetjSolver(gmo, idx);
            if( idx < 0 || idx >= gmoN(gmo) )
               symbols.back().type = Symbol::None;
         }
         else if( symType == dcteqnSymType )
         {
            // FIXME this prints a message if the equ is the objective that has been reformulated out, but then returns -2
            idx = gmoGetiSolver(gmo, idx);
            if( idx < 0 || idx >= gmoM(gmo) )
               symbols.back().type = Symbol::None;
         }
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

void analyzeMatrix(
   gmoHandle_t gmo,
   dctHandle_t dct
   )
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
         dctColUels(dct, gmoGetjModel(gmo, colidx), &colSymIdx, colUels, &colSymDim);

         if( coefs.count(std::pair<int,int>(rowSymIdx, colSymIdx)) == 0 )
         {
            coefs.insert(std::pair<std::pair<int,int>, Coefficient>(std::pair<int,int>(rowSymIdx, colSymIdx), Coefficient(symbols[rowSymIdx], symbols[colSymIdx])));
         }

         Coefficient& c(coefs.at(std::pair<int,int>(rowSymIdx, colSymIdx)));
         c.entries.push_back(std::tuple<int, int, double>(gmoGetiModel(gmo, rowidx), gmoGetjModel(gmo, colidx), jacval));

         gmoGetRowJacInfoOne(gmo, rowidx, &jacptr, &jacval, &colidx, &nlflag);
      }
   }
}

void analyzeObjective(
   gmoHandle_t gmo,
   dctHandle_t dct
   )
{
   int symIndex;
   int uelIdxs[GMS_MAX_INDEX_DIM];
   int dim;

   int nz = gmoNZ(gmo);
   int* colidx = new int[nz];
   double* jacval = new double[nz];

   gmoGetObjSparse(gmo, colidx, jacval, NULL, &nz, &dim);

   for( int i = 0; i < nz; ++i )
   {
      dctColUels(dct, colidx[i], &symIndex, uelIdxs, &dim);

      if( coefs.count(std::pair<int,int>(0, symIndex)) == 0 )
      {
         coefs.insert(std::pair<std::pair<int,int>, Coefficient>(std::pair<int,int>(0, symIndex), Coefficient(symbols[0], symbols[symIndex])));
      }

      Coefficient& c(coefs.at(std::pair<int,int>(0, symIndex)));
      c.entries.push_back(std::tuple<int, int, double>(gmoObjRow(gmo), gmoGetjModel(gmo, colidx[i]), jacval[i]));
   }

   delete[] colidx;
   delete[] jacval;

#if 0
   int objvar = gmoObjVar(gmo);

   dctColUels(dct, objvar, &symIndex, uelIdxs, &dim);

   Symbol& sym(symbols.at(symIndex));

   assert(sym.dim() == 0);
   assert(sym.type == Symbol::Variable);

   sym.type = Symbol::Objective;
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

   for( auto& e : symbols )
   {
      if( e.type == Symbol::None )
         continue;

      if( e.dim() == 0 )
         continue;

      std::string indexname = e.name + "_index";
      std::cout << indexname << ":" << std::endl;
      w.Key(indexname);

      w.StartObject();
      for( int d = 0; d < e.dim(); ++d )
      {
         std::string key = std::string("*") + e.getDomName(d);
         std::cout << "  " << key << ": String" << std::endl;
         w.Key(key);
         w.String("String");
      }

      if( e.type == Symbol::Constraint )
      {
         std::cout << "  rhs: Double" << std::endl;
         w.Key("rhs");
         w.String("Double");
      }

      w.EndObject();
   }

   for( auto& cit : coefs )
   {
      Coefficient& c(cit.second);

      std::cout << c.getName() << ":" << std::endl;
      w.Key(c.getName());

      w.StartObject();
      for( int r = 0; r < c.equation.dim(); ++r )
      {
         std::string key = std::string("*") + c.equation.getDomName(r);
         std::cout << "  " << key << ": String" << std::endl;
         w.Key(key);
         w.String("String");
      }

      for( int cd = 0; cd < c.variable.dim(); ++cd )
      {
         std::cout << "  ";
         if( c.varDomEqualsEquDom[cd] >= 0 )
             std::cout << "(";
         std::string key = std::string("*") + c.variable.getDomName(cd);
         std::cout << key << ": String";
         if( c.varDomEqualsEquDom[cd] >= 0 )
            std::cout << ")";
         std::cout << std::endl;

         if( c.varDomEqualsEquDom[cd] < 0 )
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
   gmoHandle_t gmo,
   dctHandle_t dct
   )
{
   int uelIndices[GMS_MAX_INDEX_DIM];
   char uelLabel[GMS_SSSIZE];

   for( auto& e : symbols )
   {
      if( e.dim() == 0 )
         continue;

      if( e.type == Symbol::None )
         continue;

      std::string indexname = e.name + "_index";
      std::cout << indexname << ":" << std::endl;
      w.Key(indexname);

      w.StartArray();

      for( int idx = dctSymOffset(dct, e.symIdx); ; ++idx )
      {
         int symIndex;
         int symDim;
         assert(idx >= 0);

         if( e.type == Symbol::Variable )
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

         assert(symDim == e.dim());

         w.StartObject();
         for( int d = 0; d < e.dim(); ++d )
         {
            uelLabel[0] = '\0';
            dctUelLabel(dct, uelIndices[d], uelLabel, uelLabel, sizeof(uelLabel));

            std::string key = e.getDomName(d);
            std::cout << "  " << key << ":" << uelLabel;

            w.Key(key);
            w.String(uelLabel);
         }

         if( e.type == Symbol::Constraint )
         {
            double rhs = gmoGetRhsOne(gmo, gmoGetiSolver(gmo, idx));
            std::cout << "  rhs:" << rhs;

            w.Key("rhs");
            w.Double(rhs);
         }

         std::cout << std::endl;
         w.EndObject();
      }

      w.EndArray();
   }
}

void printCoefficientData(
   rapidjson::PrettyWriter<rapidjson::StringBuffer>& w,
   dctHandle_t dct
   )
{
   int rowUels[GMS_MAX_INDEX_DIM];
   int colUels[GMS_MAX_INDEX_DIM];

   char uelLabel[GMS_SSSIZE];

   for( auto& cit : coefs )
   {
      Coefficient& c(cit.second);

      std::cout << c.getName() << ":" << std::endl;
      w.Key(c.getName());
      w.StartArray();

      for( auto& e : c.entries )
      {
         int symidx;
         int dim;
         dctRowUels(dct, std::get<0>(e), &symidx, rowUels, &dim);
         dctColUels(dct, std::get<1>(e), &symidx, colUels, &dim);

         w.StartObject();
         for( int d = 0; d < c.equation.dim(); ++d )
         {
            uelLabel[0] = '\0';
            dctUelLabel(dct, rowUels[d], uelLabel, uelLabel, sizeof(uelLabel));

            std::string key = c.equation.getDomName(d);
            std::cout << "  " << key << ":" << uelLabel;
            w.Key(key);
            w.String(uelLabel);
         }
         std::cout << ", ";

         for( int d = 0; d < c.variable.dim(); ++d )
         {
            uelLabel[0] = '\0';
            dctUelLabel(dct, colUels[d], uelLabel, uelLabel, sizeof(uelLabel));

            std::cout << "  ";
            if( c.varDomEqualsEquDom[d] >= 0 )
               std::cout << '(';
            std::string key = c.variable.getDomName(d);
            std::cout << key << ":" << uelLabel;
            if( c.varDomEqualsEquDom[d] >= 0 )
               std::cout << ')';

            if( c.varDomEqualsEquDom[d] < 0 )
            {
               w.Key(key);
               w.String(uelLabel);
            }
         }
         std::cout << ", ";

         std::cout << "val:" << std::get<2>(e) << std::endl;
         w.Key("val");
         w.Double(std::get<2>(e));

         w.EndObject();
      }

      w.EndArray();
   }
}

void printSymbols(
   rapidjson::PrettyWriter<rapidjson::StringBuffer>& w,
   gmoHandle_t gmo,
   dctHandle_t dct,
   int type
   )
{
   std::cout << std::endl;
   if( type == Symbol::Variable )
   {
      std::cout << "VARIABLES:" << std::endl;
      w.Key("VARIABLES");
   }
   else if( type == Symbol::Constraint )
   {
      std::cout << "CONSTRAINTS:" << std::endl;
      w.Key("CONSTRAINTS");
   }
   else
   {
      std::cout << "DECISION_EXPRESSIONS:" << std::endl;
      w.Key("DECISION_EXPRESSIONS");
   }

   w.StartArray();
   for( auto& e : symbols )
   {
      if( e.type != type )
         continue;

      w.StartObject();

      std::cout << "Name: " << e.name << std::endl;
      w.Key("Name");
      w.String(e.name);

      w.Key("Index");
      if( e.dim() > 0 )
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

      if( e.type == Symbol::Constraint )
      {
         int uelIndices[GMS_MAX_INDEX_DIM];
         memset(uelIndices, 0, sizeof(uelIndices));
         int rowidx;
         void* ret = dctFindFirstRowCol(dct, e.symIdx, uelIndices, &rowidx);
         assert(ret != NULL);
         rowidx = gmoGetiSolver(gmo, rowidx);
         assert(rowidx >= 0 && rowidx < gmoM(gmo));

         std::cout << "RHS: ";
         w.Key("RHS");
         if( e.dim() > 0 )
         {
            std::cout << e.name << "_index.rhs" << std::endl;
            w.String(e.name + "_index.rhs");
         }
         else
         {
            std::cout << gmoGetRhsOne(gmo, rowidx) << std::endl;
            w.Double(gmoGetRhsOne(gmo, rowidx));
         }

         std::cout << "SENSE: ";
         w.Key("SENSE");
         switch( gmoGetEquTypeOne(gmo, rowidx) )
         {
            case gmoequ_E :
            case gmoequ_B :
               std::cout << "==" << std::endl;;
               w.String("==");
               break;
            case gmoequ_G :
               std::cout << ">=" << std::endl;;
               w.String(">=");
               break;
            case gmoequ_L :
               std::cout << "<=" << std::endl;;
               w.String("<=");
               break;
            default:
               std::cout << "N/A" << std::endl;
               std::cerr << "Unsupported equation type" << std::endl;
               w.String("UNSUPPORTED");
               break;
         }
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
   w.Key("COEFFICIENTS");

   w.StartArray();
   for( auto& cit : coefs )
   {
      Coefficient& c(cit.second);

      w.StartObject();

      std::cout << "Constraints: " << c.equation.name << std::endl;
      w.Key("Constraints");
      w.String(c.equation.name);

      std::cout << "Variables: " << c.variable.name << std::endl;
      w.Key("Variables");
      w.String(c.variable.name);

      std::string entry = c.getName() + ".val";
      std::cout << "Entries: " << entry << std::endl;
      w.Key("Entries");
      w.String(entry);

      std::string cond;
      for( int d = 0; d < c.variable.dim(); ++d )
      {
         if( c.varDomEqualsEquDom[d] >= 0 )
         {
            if( cond != "" )
               cond += " and ";
            cond += c.variable.name + "." + c.variable.getDomName(d) + " == ";
            cond += c.equation.name + "." + c.equation.getDomName(d);
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
   gmoObjReformSet(gmo, 1);
   gmoObjStyleSet(gmo, gmoObjType_Fun);
   gmoIndexBaseSet(gmo, 0);

   dct = (dctHandle_t)gmoDict(gmo);
   assert(dct != NULL);

   analyzeDict(gmo, dct);
   analyzeMatrix(gmo, dct);
   analyzeObjective(gmo, dct);
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
   printIndexData(writer, gmo, dct);
   printCoefficientData(writer, dct);
   writer.EndObject();

   printSymbols(writer, gmo, dct, Symbol::Variable);
   printSymbols(writer, gmo, dct, Symbol::Constraint);
   printSymbols(writer, gmo, dct, Symbol::Objective);
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
