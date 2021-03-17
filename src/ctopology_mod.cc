//=========================================================================
//  CTOPOLOGY.CC - part of
//
//                  OMNeT++/OMNEST
//           Discrete System Simulation in C++
//
//   Member functions of
//     cTopology_mod : network topology to find shortest paths etc.
//
//  Author: Andras Varga
//
//=========================================================================

/*--------------------------------------------------------------*
  Copyright (C) 1992-2015 Andras Varga
  Copyright (C) 2006-2015 OpenSim Ltd.

  This file is distributed WITHOUT ANY WARRANTY. See the file
  `license' for details on this and other legal matters.
*--------------------------------------------------------------*/

#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <deque>
#include <list>
#include <map>
#include <fstream>
#include <algorithm>
#include <sstream>
#include "omnetpp/cpatternmatcher.h"
#include "omnetpp/cpar.h"
#include "omnetpp/globals.h"
#include "omnetpp/cexception.h"
#include "omnetpp/cproperty.h"
#include "ctopology_mod.h"

#ifdef WITH_PARSIM
#include "omnetpp/ccommbuffer.h"
#endif

using namespace omnetpp::common;

namespace omnetpp {

Register_Class(cTopology_mod);


cTopology_mod::LinkIn *cTopology_mod::Node::getLinkIn(int i)
{
    if (i < 0 || i >= (int)inLinks.size())
        throw cRuntimeError("cTopology_mod::Node::getLinkIn: invalid link index %d", i);
    return (cTopology_mod::LinkIn *)inLinks[i];
}

cTopology_mod::LinkOut *cTopology_mod::Node::getLinkOut(int i)
{
    if (i < 0 || i >= (int)outLinks.size())
        throw cRuntimeError("cTopology_mod::Node::getLinkOut: invalid index %d", i);
    return (cTopology_mod::LinkOut *)outLinks[i];
}

//----

cTopology_mod::cTopology_mod(const char *name) : cOwnedObject(name)
{
    target = nullptr;
}

cTopology_mod::cTopology_mod(const cTopology_mod& topo) : cOwnedObject(topo)
{
    throw cRuntimeError(this, "copy ctor not implemented yet");
}

cTopology_mod::~cTopology_mod()
{
    clear();
}

#if 0
std::string cTopology_mod::info() const
{
    std::stringstream out;
    out << "n=" << nodes.size();
    return out.str();
}
#endif

void cTopology_mod::parsimPack(cCommBuffer *buffer) const
{
    throw cRuntimeError(this, "parsimPack() not implemented");
}

void cTopology_mod::parsimUnpack(cCommBuffer *buffer)
{
    throw cRuntimeError(this, "parsimUnpack() not implemented");
}

cTopology_mod& cTopology_mod::operator=(const cTopology_mod&)
{
    throw cRuntimeError(this, "operator= not implemented yet");
}

void cTopology_mod::clear()
{
    for (int i = 0; i < (int)nodes.size(); i++) {
        for (int j = 0; j < (int)nodes[i]->outLinks.size(); j++)
            delete nodes[i]->outLinks[j];  // delete links from their source side
        delete nodes[i];
    }
    nodes.clear();
}

//---

static bool selectByModulePath(cModule *mod, void *data)
{
    // actually, this is selectByModuleFullPathPattern()
    const std::vector<std::string>& v = *(const std::vector<std::string> *)data;
    std::string path = mod->getFullPath();
    for (int i = 0; i < (int)v.size(); i++)
        if (cPatternMatcher(v[i].c_str(), true, true, true).matches(path.c_str()))
            return true;

    return false;
}

static bool selectByNedTypeName(cModule *mod, void *data)
{
    const std::vector<std::string>& v = *(const std::vector<std::string> *)data;
    return std::find(v.begin(), v.end(), mod->getNedTypeName()) != v.end();
}

static bool selectByProperty(cModule *mod, void *data)
{
    struct ParamData {const char *name; const char *value;};
    ParamData *d = (ParamData *)data;
    cProperty *prop = mod->getProperties()->get(d->name);
    if (!prop)
        return false;
    const char *value = prop->getValue(cProperty::DEFAULTKEY, 0);
    if (d->value)
        return opp_strcmp(value, d->value) == 0;
    else
        return opp_strcmp(value, "false") != 0;
}

static bool selectByParameter(cModule *mod, void *data)
{
    struct PropertyData{const char *name; const char *value;};
    PropertyData *d = (PropertyData *)data;
    return mod->hasPar(d->name) && (d->value == nullptr || mod->par(d->name).str() == std::string(d->value));
}

//---

void cTopology_mod::extractByModulePath(const std::vector<std::string>& fullPathPatterns)
{
    extractFromNetwork(selectByModulePath, (void *)&fullPathPatterns);
}

void cTopology_mod::extractByNedTypeName(const std::vector<std::string>& nedTypeNames)
{
    extractFromNetwork(selectByNedTypeName, (void *)&nedTypeNames);
}

void cTopology_mod::extractByProperty(const char *propertyName, const char *value)
{
    struct {const char *name; const char *value;} data = {propertyName, value};
    extractFromNetwork(selectByProperty, (void *)&data);
}

void cTopology_mod::extractByParameter(const char *paramName, const char *paramValue)
{
    struct {const char *name; const char *value;} data = {paramName, paramValue};
    extractFromNetwork(selectByParameter, (void *)&data);
}

//---

static bool selectByPredicate(cModule *mod, void *data)
{
    cTopology_mod::Predicate *predicate = (cTopology_mod::Predicate *)data;
    return predicate->matches(mod);
}

void cTopology_mod::extractFromNetwork(Predicate *predicate)
{
    extractFromNetwork(selectByPredicate, (void *)predicate);
}

void cTopology_mod::extractFromNetwork(bool (*predicate)(cModule *, void *), void *data)
{
    clear();

    // Loop through all modules and find those that satisfy the criteria
    for (int moduleId = 0; moduleId <= getSimulation()->getLastComponentId(); moduleId++) {
        cModule *module = getSimulation()->getModule(moduleId);
        if (module && predicate(module, data)) {
            Node *node = createNode(module);
            nodes.push_back(node);
        }
    }

    // Discover out neighbors too.
    for (int k = 0; k < (int)nodes.size(); k++) {
        // Loop through all its gates and find those which come
        // from or go to modules included in the topology.

        Node *node = nodes[k];
        cModule *module = getSimulation()->getModule(node->moduleId);

        for (cModule::GateIterator it(module); !it.end(); ++it) {
            cGate *gate = *it;
            if (gate->getType() != cGate::OUTPUT)
                continue;

            // follow path
            cGate *srcGate = gate;
            do {
                gate = gate->getNextGate();
            } while (gate && !predicate(gate->getOwnerModule(), data));

            // if we arrived at a module in the topology, record it.
            if (gate) {
                Link *link = createLink();
                link->srcNode = node;
                link->srcGateId = srcGate->getId();
                link->destNode = getNodeFor(gate->getOwnerModule());
                link->destGateId = gate->getId();
                node->outLinks.push_back(link);
            }
        }
    }

    // fill inLinks vectors
    for (int k = 0; k < (int)nodes.size(); k++) {
        for (int l = 0; l < (int)nodes[k]->outLinks.size(); l++) {
            cTopology_mod::Link *link = nodes[k]->outLinks[l];
            link->destNode->inLinks.push_back(link);
        }
    }
}

int cTopology_mod::addNode(Node *node)
{
    if (node->moduleId == -1) {
        // elements without module ID are stored at the end
        nodes.push_back(node);
        return nodes.size()-1;
    }
    else {
        // must find an insertion point because nodes[] is ordered by module ID
        std::vector<Node *>::iterator it = std::lower_bound(nodes.begin(), nodes.end(), node, lessByModuleId);
        it = nodes.insert(it, node);
        return it - nodes.begin();
    }
}

void cTopology_mod::deleteNode(Node *node)
{
    // remove outgoing links
    for (int i = 0; i < (int)node->outLinks.size(); i++) {
        Link *link = node->outLinks[i];
        unlinkFromDestNode(link);
        delete link;
    }
    node->outLinks.clear();

    // remove incoming links
    for (int i = 0; i < (int)node->inLinks.size(); i++) {
        Link *link = node->inLinks[i];
        unlinkFromSourceNode(link);
        delete link;
    }
    node->inLinks.clear();

    // remove from nodes[]
    std::vector<Node *>::iterator it = std::find(nodes.begin(), nodes.end(), node);
    ASSERT(it != nodes.end());
    nodes.erase(it);

    delete node;
}

void cTopology_mod::addLink(Link *link, Node *srcNode, Node *destNode)
{
    // remove from graph if it's already in
    if (link->srcNode)
        unlinkFromSourceNode(link);
    if (link->destNode)
        unlinkFromDestNode(link);

    // insert
    if (link->srcNode != srcNode)
        link->srcGateId = -1;
    if (link->destNode != destNode)
        link->destGateId = -1;
    link->srcNode = srcNode;
    link->destNode = destNode;
    srcNode->outLinks.push_back(link);
    destNode->inLinks.push_back(link);
}

void cTopology_mod::addLink(Link *link, cGate *srcGate, cGate *destGate)
{
    // remove from graph if it's already in
    if (link->srcNode)
        unlinkFromSourceNode(link);
    if (link->destNode)
        unlinkFromDestNode(link);

    // insert
    Node *srcNode = getNodeFor(srcGate->getOwnerModule());
    Node *destNode = getNodeFor(destGate->getOwnerModule());
    if (!srcNode)
        throw cRuntimeError("cTopology_mod::addLink: module of source gate \"%s\" is not in the graph", srcGate->getFullPath().c_str());
    if (!destNode)
        throw cRuntimeError("cTopology_mod::addLink: module of destination gate \"%s\" is not in the graph", destGate->getFullPath().c_str());
    link->srcNode = srcNode;
    link->destNode = destNode;
    link->srcGateId = srcGate->getId();
    link->destGateId = destGate->getId();
    srcNode->outLinks.push_back(link);
    destNode->inLinks.push_back(link);
}

void cTopology_mod::deleteLink(Link *link)
{
    unlinkFromSourceNode(link);
    unlinkFromDestNode(link);
    delete link;
}

void cTopology_mod::unlinkFromSourceNode(Link *link)
{
    std::vector<Link *>& srcOutLinks = link->srcNode->outLinks;
    std::vector<Link *>::iterator it = std::find(srcOutLinks.begin(), srcOutLinks.end(), link);
    ASSERT(it != srcOutLinks.end());
    srcOutLinks.erase(it);
}

void cTopology_mod::unlinkFromDestNode(Link *link)
{
    std::vector<Link *>& destInLinks = link->destNode->inLinks;
    std::vector<Link *>::iterator it = std::find(destInLinks.begin(), destInLinks.end(), link);
    ASSERT(it != destInLinks.end());
    destInLinks.erase(it);
}

cTopology_mod::Node *cTopology_mod::getNode(int i)
{
    if (i < 0 || i >= (int)nodes.size())
        throw cRuntimeError(this, "invalid node index %d", i);
    return nodes[i];
}

cTopology_mod::Node *cTopology_mod::getNodeFor(cModule *mod)
{
    // binary search because nodes[] is ordered by module ID
    Node tmpNode(mod->getId());
    std::vector<Node *>::iterator it = std::lower_bound(nodes.begin(), nodes.end(), &tmpNode, lessByModuleId);
//TODO: this does not compile with VC9 (VC10 is OK): std::vector<Node*>::iterator it = std::lower_bound(nodes.begin(), nodes.end(), mod->getId(), isModuleIdLess);
    return it == nodes.end() || (*it)->moduleId != mod->getId() ? nullptr : *it;
}

void cTopology_mod::calculateUnweightedSingleShortestPathsTo(Node *_target)
{
    // multiple paths not supported :-(

    if (!_target)
        throw cRuntimeError(this, "..ShortestPathTo(): target node is nullptr");
    target = _target;

    for (int i = 0; i < (int)nodes.size(); i++) {
        nodes[i]->dist = INFINITY;
        nodes[i]->outPath = nullptr;
    }
    target->dist = 0;

    std::deque<Node *> q;

    q.push_back(target);

    while (!q.empty()) {
        Node *v = q.front();
        q.pop_front();

        // for each w adjacent to v...
        for (int i = 0; i < (int)v->inLinks.size(); i++) {
            if (!v->inLinks[i]->enabled)
                continue;

            Node *w = v->inLinks[i]->srcNode;
            if (!w->enabled)
                continue;

            if (w->dist == INFINITY) {
                w->dist = v->dist+1;
                w->outPath = v->inLinks[i];
                w->outPaths.push_back(v->inLinks[i]);
                q.push_back(w);
            }
        }
    }
}

void cTopology_mod::calculateWeightedSingleShortestPathsTo(Node *_target)
{
    if (!_target)
        throw cRuntimeError(this, "..ShortestPathTo(): target node is nullptr");
    target = _target;

    // clean path infos
    for (int i = 0; i < (int)nodes.size(); i++) {
        nodes[i]->dist = INFINITY;
        nodes[i]->outPath = nullptr;
    }

    target->dist = 0;

    std::list<Node *> q;

    q.push_back(target);

    while (!q.empty()) {
        Node *dest = q.front();
        q.pop_front();

        ASSERT(dest->getWeight() >= 0.0);

        // for each w adjacent to v...
        for (int i = 0; i < dest->getNumInLinks(); i++) {
            if (!(dest->getLinkIn(i)->isEnabled()))
                continue;

            Node *src = dest->getLinkIn(i)->getRemoteNode();
            if (!src->isEnabled())
                continue;

            double linkWeight = dest->getLinkIn(i)->getWeight();
            ASSERT(linkWeight > 0.0);

            double newdist = dest->dist + linkWeight;
            if (dest != target)
                newdist += dest->getWeight();  // dest is not the target, uses weight of dest node as price of routing (infinity means dest node doesn't route between interfaces)
            if (newdist != INFINITY && src->dist > newdist) {  // it's a valid shorter path from src to target node
                if (src->dist != INFINITY)
                    q.remove(src);  // src is in the queue
                src->dist = newdist;
                src->outPath = dest->inLinks[i];
                src->outPaths.push_back(dest->inLinks[i]);

                // insert src node to ordered list
                std::list<Node *>::iterator it;
                for (it = q.begin(); it != q.end(); ++it)
                    if ((*it)->dist > newdist)
                        break;

                q.insert(it, src);
            }
        }
    }
}

void cTopology_mod::weightedMultiShortestPathsTo(Node *_target)
{

    if (!_target)
        throw cRuntimeError(this,"..ShortestPathTo(): target node is NULL");
    target = _target;

    // clean path infos
    for (int i = 0; i < (int)nodes.size(); i++) {
    	nodes[i]->dist = INFINITY;
    	nodes[i]->known = false;
    	nodes[i]->outPath = nullptr;
    }

    target->dist = 0;  //The distace between target and iself is 0.

	//The deque can be expanded in both the ends.
    std::deque<Node*> q;

    q.push_back(target);

    while (!q.empty())
    {
       //find shortest in q
       //For unweighted shortest paths it's the first element of q is always the node with the shortest distance
       Node *dest = q.front();
       q.pop_front();
       dest->known = true;

       // for each w adjacent to v...
       for (int i = 0; i < dest->getNumInLinks(); i++)
       {
    	   if (!(dest->getLinkIn(i)->isEnabled()))
    		   continue;

    	   Node *src = dest->getLinkIn(i)->getRemoteNode();
    	   if (!src->isEnabled())
    		   continue;

           if (src->dist == INFINITY || (src->known==false && src->dist >  dest->dist + 1  ))
           {
		       src->outPaths.clear();
               src->dist = dest->dist + dest->inLinks[i]->weight;//1;
               src->outPaths.push_back(dest->inLinks[i]);
               src->outPath = dest->inLinks[i];
               q.push_back(src);
           }
           else if ( src->dist != INFINITY && src->known==false && src->dist == dest->dist + 1)
           {
        	   src->outPaths.push_back(dest->inLinks[i]);
        	   src->outPath = dest->inLinks[i];
           }
       }
    }
}

void cTopology_mod::extractWeight(std::string file)
{
	 using namespace std;
     std::ifstream fi;
     char row[100];
     char *path;
     int o,d;
     int weightF;
     fi.open(file.c_str());

     while (fi >> row)
     {
         path = strtok(row,":");
         o = atoi(path); //origin node
         Node * source=getNode(o);
         path = strtok(NULL,":");
         d = atoi(path); //destination node
         Node * dest=getNode(d);
         path = strtok(NULL,":");
         weightF = atoi(path); //weight
         //cout<<"Link between "<<o<<" and "<<d<<" with weight = "<<weight<<endl;
         for (int i = 0; i < source->getNumOutLinks(); i++)
         {
        	 Node *try_dest = source->getLinkOut(i)->destNode;
        	 if(d==((try_dest->getModule())->getIndex()))
        	 {
        		 cTopology_mod::Link * link=source->getLinkOut(i);
        		 link->setWeight(weightF);
        	 }
         }
     }
     fi.close();
}

/* Betweenness centrality calculation. All the input parameters are provided by
 * the cTopology_mod class. The algorithm has been published on the paper "A faster
 * Algorithm for Betweenness Centrality by Ulrik Brandes and has a complexity
 * of O(nm)
 */
void cTopology_mod::betweenness_centrality()
{
    std::deque<Node*> S,Q;
    std::map< int,std::deque<Node *> > P;

    Node *v;
    Node *w;

    int t; //temporary iterator

    //Betweenness is zero (for each node) at the beginning
    for (int i = 0; i < (int)nodes.size(); i++)
    	nodes[i]->btw = 0;

    int s = 0;

    for (int i = 0; i < (int)nodes.size(); i++)
    {
    	//
    	//For each source reinitialize the variables
    	//i.e., repeat the algorithm for each source
    	//
    	S.clear();
    	Q.clear();

    	for (int i = 0; i < (int)nodes.size(); i++)
    	{
    		P.clear();
    		nodes[t]->sigma = 0;
    		nodes[t]->delta = 0;
    		nodes[t]->dist = -1;
    	}

    	nodes[s]->sigma = 1;
    	nodes[s]->dist  = 0;

    	//
    	//Node v is the (pointer to the) current node
    	//Node w is the (pointer to the) neighbour
    	//
    	Q.push_back(nodes[s]);
    	while (!Q.empty())
    	{
    		Node *v = Q.front(); // given the node v
    		Q.pop_front();
    		//std::cout<<"popping "<<v->module_id<<endl;

    		S.push_back(v);
    		for (int i = 0; i < v->getNumInLinks(); i++)
    		{
    			w = v->getLinkIn(i)->srcNode;
    			if ( w->dist < 0)
    			{
    				//std::cout<<"enqueuing "<< w->module_id << endl;
    				w->dist = v->dist+1;
    				Q.push_back(w);
    			}

    			if (w->dist == v->dist + 1)
    			{
    				w->sigma += v->sigma;
    				P[w->moduleId].push_back(v); //See Lemma 3 of [brandes01jms]
    			}
    		}
    	}

    	//
    	//Node w is the (pointer to the) current node
    	//Node v is the (pointer to the) neighbour
    	//
    	while (!S.empty())
    	{
    		w = S.back();
    		S.pop_back();

    		for (std::deque<Node *>::iterator iv = P[w->moduleId].begin(); iv != P[w->moduleId].end(); iv++)
    		{
    			v = *iv;
    			v->delta += (v->sigma/w->sigma)*(1+w->delta); //See Theorem 6 of [brandes01jms]
    		}

    		if (w->moduleId != nodes[s]->moduleId)
    			w->btw += w->delta;
    	}
    }
}

}  // namespace omnetpp

