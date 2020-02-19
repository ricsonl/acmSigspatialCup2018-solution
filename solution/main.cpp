#include <iostream>
#include <fstream>
#include <map>
#include <algorithm>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/biconnected_components.hpp>
#include <boost/graph/connected_components.hpp>
#include "boost/graph/graph_traits.hpp"
#include <boost/foreach.hpp>
#include <string>
#include "include/rapidjson/document.h"

using namespace std;

struct edge_component_t{//struct necessaria para o funcionamento da funcao biconnected_components
  enum
  { num = 555 };
  typedef boost::edge_property_tag kind;
}
edge_component;

typedef boost::adjacency_list<boost::vecS,boost::vecS,boost::undirectedS,boost::no_property, boost::property <edge_component_t, std::size_t > > Graph;//lista de adjacencia representando o grafo do problema
typedef boost::graph_traits<Graph>::edge_iterator edge_iterator;//iterador que ira iterar sobre as arestas
typedef boost::graph_traits<Graph>::vertex_descriptor vertex_descriptor;//funciona como um ponteiro para vertices do grafo
typedef boost::graph_traits<Graph>::edge_descriptor edge_descriptor;//funciona como um ponteiro para arestas do grafo

//funcao que lera o grafo a partir do json:
void leGrafo(Graph &g, const string &js, const string &txt, vector<bool> &startVec, vector<bool>&controlVec, map<string, int> &vDictIN, map<int, string> &vDictOUT, multimap<string, pair<int,int> > &edgesMapIN, map<pair<int,int>, string> &edgesMapOUT, int &V){
  rapidjson::Document doc;
  doc.Parse(js.c_str());
  auto &rows = doc["rows"];
  for(auto &obj : rows.GetArray()){
    string u = obj["fromGlobalId"].GetString();//vertice de partida
    string v = obj["toGlobalId"].GetString();//vertice de chegada
		string e = obj["viaGlobalId"].GetString();//via
    if(vDictIN.insert(make_pair(u,V)).second){//para o vertice from, se ele ainda nao tiver sido inserido:
      boost::add_vertex(g);//criamos um vertice solto no grafo g
      vDictOUT.insert(make_pair(V++,u));//inserimos o vertice no dicionario de saida e incrementamos a variavel de indice
    }
    if(vDictIN.insert(make_pair(v,V)).second){//para o vertice to, se ele ainda nao tiver sido inserido:
      boost::add_vertex(g);//criamos um vertice solto no grafo g
      vDictOUT.insert(make_pair(V++,v));//inserimos o vertice no dicionario de saida e incrementamos a variavel de indice
    }
    //teremos que inserir a aresta de qualquer forma, pois nao ha arestas repetidas no json.
    boost::add_edge(vDictIN[u], vDictIN[v], g);//ligue a aresta ao vertice u e ao vertice v
    edgesMapIN.insert(make_pair(e, make_pair(vDictIN[u], vDictIN[v])));//inserimos a aresta no dicionario de entrada, de string para pair<int,int>
    edgesMapOUT.insert(make_pair(make_pair(vDictIN[u], vDictIN[v]), e));//inserimos a aresta no dicionario de saida, de pair<int,int> para string
  }

  controlVec = vector<bool>(V,false);//vector que informara' quais vertices sao controladores
  startVec = vector<bool>(V,false);//vector que informara' quais vertices sao starting points

  auto &control = doc["controllers"];
  for(auto &val : control.GetArray()){
    string c = val["globalId"].GetString();
    controlVec[vDictIN[c]] = true;//setamos o controlvec na posicao daquele vertice para true
    vDictOUT[vDictIN[c]]=c;//inserimos seu nome no dicionario de saida
  }

  ifstream startingpoints(txt);//arquivo que contem os starting points
  while(true){//para cada starting point
    string st;
    startingpoints >> st;
    if(st.empty()) break;
    if(edgesMapIN.find(st)!=edgesMapIN.end()){//se o starting point for uma aresta:
      boost::add_vertex(g);//criamos um vertice auxiliar que ira representa-la
      multimap<string, pair<int,int> >::iterator it = edgesMapIN.find(st);//encontramos a aresta que possui o id inserido (st)
      int u = (*it).second.first;//:
      int w = (*it).second.second;//pegamos os dois vertices adjacentes a ela
      boost::remove_edge(u, w, g);//removemos a aresta (entre u e w)
      boost::add_edge(V, u, g);//:
      boost::add_edge(V, w, g);//conectamos o novo vertice aos dois vertices aos quais a aresta estava conectada
      vDictIN.insert(make_pair(st, V));//:
      vDictOUT.insert(make_pair(V,st));//o inserimos no dicionario de vertices normalmente, porem com o nome da aresta
    }
    else//se o starting point for um vertice
      startVec[vDictIN[st]] = true;//apenas atribua true ao startVec naquela posicao

    if(startingpoints.eof()) break;
  }
}

//funcao que adiciona as arestas auxiliares, cruciais na resolucao do problema:
void addAux(Graph &g, const vector<bool> &startVec, const vector<bool> &controlVec, const vector<int> &cc, int numCC, set<edge_descriptor> &auxEdges, int &V){
  boost::add_vertex(g);//criamos um vertice que sera ligado a todos os starting points
  int s = V++;//pegamos o indice dele e incrementamos a variavel de indice
  boost::add_vertex(g);//criamos um vertice que sera ligado a todos os controllers
  int c = V++;//pegamos o indice dele e incrementamos a variavel de indice
  bool encontrouS=false;//indica se foi encontrado pelo menos um starting point
  bool encontrouC=false;//indica se foi encontrado pelo menos um controller

  int tam = startVec.size();//numero total de vertices
  for(int i=0; i<tam; ++i){//para cada vertice do CC (tirando os dois que acabaram de ser criados)
    if(startVec[i]&&cc[i]==numCC){//se o vertice for starting point e for do componente conexo passado como argumento, crie a aresta conectando ele ao starting point principal (s)
      encontrouS=true;
      add_edge(i, s, g);//adiciona uma aresta entre tal starting point e o principal (s)
    }
    if(controlVec[i]&&cc[i]==numCC){//se o vertice for controller e for do componente conexo passado como argumento, crie a aresta conectando ele ao controller principal (c)
      encontrouC=true;
      add_edge(i, c, g);//adiciona uma aresta entre tal controller e o principal (c)
    }
  }
  if(encontrouS&&encontrouC){//se tiver encontrado pelo menos um starting point e um controller
    add_edge(s, c, g);//conecte os dois vertices auxiliares s e c.
    pair<edge_descriptor, bool> auxPair = edge(vertex_descriptor(s), vertex_descriptor(c), g);//o "first" de auxPair ira armazenar um edge descriptor (semelhante a um ponteiro) para a aresta, e isso sera armazenado em auxEdges
    auxEdges.insert(auxPair.first);//armazene o descriptor da aresta auxiliar
  }
}

int main(int argc, char **argv){
  string str;//string que recebera o conteudo do json, para ser lida pelo rapidjson na funcao leGrafo
  ifstream in(argv[1]);//arquivo json a ser lido
  getline(in,str,(char)-1);//converta todo o json para string e armazene em str

  Graph g;//criacao do grafo
  map<string, int> vDictIN;//dicionario que ira converter o nome de cada vertice para seu indice
  map<int, string> vDictOUT;//dicionario que ira converter o indice de cada vertice para seu nome
  multimap<string, pair<int,int> > edgesMapIN;//dicionario que ira converter o nome de cada aresta para um pair com seus vertices
  map<pair<int,int>, string> edgesMapOUT;//dicionario que ira converter o pair com dois vertices para o nome da aresta entre eles

  ofstream out(argv[3]);
	int V=0;//representara o indice de cada vertice
  vector<bool>controlVec;//vector que indica quais vertices sao controladores
	vector<bool>startVec;//vector que indica quais vertices sao starting points

  leGrafo(g,str,argv[2],startVec,controlVec,vDictIN,vDictOUT,edgesMapIN,edgesMapOUT,V);
  vector<int>cc(V);//armazena, no indice de cada vetor, o componente conexo ao qual ele pertence
  set<int> ccNums;//armazena o numero dos componentes conexos
  boost::connected_components(g, &cc[0]);//funcao que armazena em cc o numero do componente conexo de cada vertice
  for(int i=0; i<V; i++)//armazena todos os numeros de componentes conexos em ccNums
    ccNums.insert(cc[i]);

  set<edge_descriptor> auxEdges;//armazenara o edge descriptor de todas as arestas que forem auxiliares
  BOOST_FOREACH(int i, set<int>(ccNums)){//similar ao range-based for do c++11: para cada inteiro i em ccNums...
    addAux(g,startVec,controlVec,cc,i,auxEdges,V);//crie a aresta auxiliar nele
  }

  boost::property_map<Graph, edge_component_t>::type component = get(edge_component, g);//necessario para o funcionamento da funcao biconnected_components
  boost::biconnected_components(g, component);//armazena em component o numero do componente biconectado de cada aresta

  set<int> solComps;//armazena o numero dos componentes biconectados que fazem parte da solucao
  BOOST_FOREACH(edge_descriptor ei, set<edge_descriptor>(auxEdges)){//para cada aresta auxiliar (em auxEdges),
    solComps.insert(component[ei]);//adicione seu componente biconectado 'a solucao
  }

  set<int>::iterator solFim = solComps.end();
  for(pair<edge_iterator, edge_iterator> ei = boost::edges(g); ei.first != ei.second; ++ei.first){//para cada aresta do grafo
    if(solComps.find(component[*ei.first])!=solFim){//se ela pertencer a algum dos componentes biconectados da solucao:
      int u = source(*ei.first,g);//indice do vertice de partida
      int v = target(*ei.first,g);//indice do vertice alvo
      if(!vDictOUT[u].empty()) out << vDictOUT[u] << '\n';//:
      if(!vDictOUT[v].empty()) out << vDictOUT[v] << '\n';//imprima o nome de seus dois vertices
      if(!edgesMapOUT[make_pair(u,v)].empty()) out << edgesMapOUT[make_pair(u,v)] << '\n';//imprima seu proprio nome
    }
  }
  return 0;
}
