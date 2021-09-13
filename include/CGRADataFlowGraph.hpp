/*
*    MIT License
*    
*    Copyright (c) 2021 Amano laboratory, Keio University & Processor Research Team, RIKEN Center for Computational Science
*    
*    Permission is hereby granted, free of charge, to any person obtaining a copy of
*    this software and associated documentation files (the "Software"), to deal in
*    the Software without restriction, including without limitation the rights to
*    use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
*    of the Software, and to permit persons to whom the Software is furnished to do
*    so, subject to the following conditions:
*    
*    The above copyright notice and this permission notice shall be included in all
*    copies or substantial portions of the Software.
*    
*    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*    SOFTWARE.
*    
*    File:          /include/CGRADataFlowGraph.hpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
*    Created Date:  27-08-2021 15:03:28
*    Last Modified: 13-09-2021 16:48:42
*/
#ifndef CGRADataFlowGraph_H
#define CGRADataFlowGraph_H

#include "llvm/Support/DOTGraphTraits.h"
#include "llvm/ADT/DirectedGraph.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringMap.h"

#include "CGRAInstMap.hpp"


using namespace CGRAOmp;
using namespace std;

#define VROOT_NODE_ID (-1)


namespace llvm {
	class DFGNode;
	class DFGEdge;
	using DFGNodeBase = DGNode<DFGNode, DFGEdge>;
	using DFGEdgeBase = DGEdge<DFGNode, DFGEdge>;
	using CGRADFGBase = DirectedGraph<DFGNode, DFGEdge>;

	/**
	 * @class DFGNode
	 * @brief An abstract class for DFG node derived from DGNode
	*/
	class DFGNode : public DFGNodeBase {
		public:
			enum class NodeKind {
				Compute,
				MemAccess,
				Compare,
				VirtualRoot,
			};

			DFGNode(int ID, NodeKind kind, InstMapEntry *map_entry) :
				DFGNodeBase(), ID(ID), kind(kind),
				map_entry(map_entry) {};

			DFGNode(const DFGNode &N) {
				*this = N;
			}
			DFGNode(DFGNode &&N) {
				*this = std::move(N);
			}

			DFGNode &operator=(const DFGNode &N) {
				DGNode::operator=(N);
				return *this;
			}

			DFGNode &operator=(DFGNode &&N) {
				DGNode::operator=(std::move(N));
				return *this;
			}

			int getID() const { return ID; }

			virtual string getUniqueName() = 0;

		protected:
			int ID;
			NodeKind kind;
			InstMapEntry *map_entry;
	};

	/**
	 * @class VirtualRootNode
	 * @brief A concrete class for virtual root node, connected to all the primary input node.
	*/
	class VirtualRootNode : public DFGNode {
		public:
			VirtualRootNode() :
				DFGNode(VROOT_NODE_ID, 
					DFGNode::NodeKind::VirtualRoot, nullptr) {}
			string getUniqueName() {
				return "VROOT";
			}
	};

	/**
	 * @class ComputeNode
	 * @brief A concrete class for computational nodes
	*/
	class ComputeNode : public DFGNode {
		public:
			ComputeNode(int ID, InstMapEntry *map_entry) : 
				DFGNode(ID, DFGNode::NodeKind::Compute, map_entry) {}

			string getUniqueName() {
				return map_entry->getMapName() + "_" + to_string(getID());
			}
		private:

	};

	/**
	 * @class DFGEdge
	 * @brief Class of DFG edge derived from DGEdge
	*/
	class DFGEdge : public DFGEdgeBase {
		public:
			DFGEdge(DFGNode &N) : DFGEdgeBase(N) {}
			DFGEdge(const DFGEdge &E) : DFGEdgeBase(E) {
				*this = E;
			};
			DFGEdge(DFGEdge &&E) : DFGEdgeBase(std::move(E)) {
				*this = std::move(E);
			};

			DFGEdge &operator=(const DFGEdge &E) {
				DFGEdgeBase::operator=(E);
				return *this;
			};

			DFGEdge &operator=(DFGEdge &&E) {
				DFGEdgeBase::operator=(std::move(E));
				return *this;
			};
		private:
	};

	/**
	 * @class CGRADFG
	 * @brief A graph class for CGRA kernel DFG derived from DirectedGraph
	 */
	class CGRADFG : public CGRADFGBase {
		public:
			using NodeType = DFGNode;
			using EdgeType = DFGEdge;

			CGRADFG() : CGRADFGBase() {
				createVirtualRoot();
			};
			CGRADFG(const CGRADFG &G) = delete;
			CGRADFG(CGRADFG &&G) : CGRADFGBase(std::move(G)) {
				virtual_root = G.virtual_root;
				G.virtual_root = nullptr;
			};
			CGRADFG(NodeType &N) : CGRADFGBase(N) {
				createVirtualRoot();
			};

			~CGRADFG() {
				delete virtual_root;
			}

			NodeType &getRoot() const {
				return *virtual_root;
			}

			bool addNode(NodeType &N);

			bool connect(NodeType &Src, NodeType &Dst, EdgeType &E);

			string convertToReadableNodeName(const string dot_string) const;

			Error saveAsDotGraph(StringRef filepath);

			void setName(const string graph_name) {
				name = graph_name;
			}

			string getName() const {
				return name;
			}

		private:

			void createVirtualRoot() {
				virtual_root = new VirtualRootNode();
				CGRADFGBase::addNode(*virtual_root);
			}
			NodeType *virtual_root = nullptr;

			string name = "";
	};

	/**
	 * @class GraphTraits<DFGNode *>
	 * @brief Specilized template of GraphTraits for DFNode
	 */
	template <>
	struct GraphTraits<DFGNode *> {
		using NodeRef = DFGNode *;

		static DFGNode *DDGGetTargetNode(DFGEdge *P) {
			return &P->getTargetNode();
		}

		using ChildIteratorType =
			mapped_iterator<DFGNode::iterator, decltype(&DDGGetTargetNode)>;
		using ChildEdgeIteratorType = DFGNode::iterator;

		static ChildIteratorType child_begin(NodeRef N) {
			return ChildIteratorType(N->begin(), &DDGGetTargetNode);
		}
		static ChildIteratorType child_end(NodeRef N) {
			return ChildIteratorType(N->end(), &DDGGetTargetNode);
		}

		static ChildEdgeIteratorType child_edge_begin(NodeRef N) {
			return N->begin();
		}

		static ChildEdgeIteratorType child_edge_end(NodeRef N) { 
			return N->end();
		}
	};

	/**
	 * @class GraphTraits<DFGNode *>
	 * @brief Specilized template of GraphTraits for DFNode
	 */
	template <>
	struct GraphTraits<CGRADFG *> : public GraphTraits<DFGNode*> {
		using nodes_iterator = CGRADFG::iterator;
		static NodeRef getEntryNode(CGRADFG *G) {
			return &G->getRoot();
		}
		static nodes_iterator nodes_begin(CGRADFG *G) {
			return G->begin();
		}
		static nodes_iterator nodes_end(CGRADFG *G) {
			return G->end();
		}
	};

	/**
	 * @class GraphTraits<const DFGNode *>
	 * @brief Specilized template  of GraphTraits for const DFNode
	 */
	template <>
	struct GraphTraits<const DFGNode *> {
		using NodeRef = const DFGNode *;

		static const DFGNode *DDGGetTargetNode(const DFGEdge *P) {
			return &P->getTargetNode();
		}

		using ChildIteratorType =
			mapped_iterator<DFGNode::iterator, decltype(&DDGGetTargetNode)>;
		using ChildEdgeIteratorType = DFGNode::iterator;

		static ChildIteratorType child_begin(const NodeRef N) {
			return ChildIteratorType(N->begin(), &DDGGetTargetNode);
		}
		static ChildIteratorType child_end(const NodeRef N) {
			return ChildIteratorType(N->end(), &DDGGetTargetNode);
		}

		static ChildEdgeIteratorType child_edge_begin(const NodeRef N) {
			return N->begin();
		}

		static ChildEdgeIteratorType child_edge_end(const NodeRef N) { 
			return N->end();
		}
	};

	/**
	 * @class GraphTraits<const CGRADFG *>
	 * @brief Specilized template of GraphTraits for const CGRADFG
	 */
	template <>
	struct GraphTraits<const CGRADFG *> : public GraphTraits<DFGNode*> {
		using nodes_iterator = CGRADFG::const_iterator;
		static NodeRef getEntryNode(const CGRADFG *G) {
			return &G->getRoot();
		}
		static nodes_iterator nodes_begin(const CGRADFG *G) {
			return G->begin();
		}
		static nodes_iterator nodes_end(const CGRADFG *G) {
			return G->end();
		}
	};

	/**
	 * @class DOTGraphTraits<const CGRADFG *>
	 * @brief Specilized template of DotGraphTraits for CGRADFG
	 * This is needed to save CGRADFG as DOT graph file.
	 */
	template<>
	struct DOTGraphTraits<const CGRADFG *> : public DefaultDOTGraphTraits {
		public:
			DOTGraphTraits(bool isSimple) : DefaultDOTGraphTraits(isSimple) {};
			DOTGraphTraits() : DefaultDOTGraphTraits(false) {};

			string getGraphName(const CGRADFG *G) {
				return G->getName();
			}

			static string getGraphProperties(const CGRADFG *G);

			static bool isNodeHidden(const DFGNode *Node, 
								const CGRADFG *G) {
				return *Node == G->getRoot();
			}

			static string getNodeLabel(const DFGNode *Node, 
								const CGRADFG *G) {
				return formatv("Label ID: {0} info: {1}", 
								Node->getID(), "");
			}

			static string getNodeIdentifierLabel(const DFGNode *Node, 
								const CGRADFG *G) {
				return "";
			}

			static string getNodeDescription(const DFGNode *Node, 
								const CGRADFG *G) {
				return formatv("desc. of ID {0}", Node->getID());
			}

			static string getNodeAttributes(const DFGNode *Node, 
								const CGRADFG *G) {
				return formatv("myattr=ID{0}", Node->getID());
			}

			static string getEdgeAttributes(const DFGNode *Node, 
					GraphTraits<DFGNode *>::ChildIteratorType I,
					  const CGRADFG *G);
		private:
			/// a default graph properties for DOT graph
			static StringMap<StringRef> default_graph_prop;
			/// a default node propterties for DOT graph
			static StringMap<StringRef> default_node_prop;
			/// a default edge propterties for DOT graph
			static StringMap<StringRef> default_edge_prop;

	};

	using CGRADFGDotGraphTraits = DOTGraphTraits<const CGRADFG *>;

};

#endif //CGRADataFlowGraph_H