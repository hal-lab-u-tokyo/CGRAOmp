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
*    Last Modified: 07-02-2022 17:32:50
*/
#ifndef CGRADataFlowGraph_H
#define CGRADataFlowGraph_H

#include "llvm/Support/DOTGraphTraits.h"
#include "llvm/ADT/DirectedGraph.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Instruction.h"
#include "llvm/ADT/APFloat.h"

#include "CGRAInstMap.hpp"
#include "OptionPlugin.hpp"

#include <string>
#include <utility>
#include <stdint.h>

using namespace CGRAOmp;
using namespace std;

#define VROOT_NODE_ID (-1)


namespace llvm {
	class DFGNode;
	class DFGEdge;
	class CGRADFG;
	using DFGNodeBase = DGNode<DFGNode, DFGEdge>;
	using DFGEdgeBase = DGEdge<DFGNode, DFGEdge>;
	using CGRADFGBase = DirectedGraph<DFGNode, DFGEdge>;

	/**
	 * @class DFGNode
	 * @brief An abstract class for DFG node derived from DGNode
	*/
	class DFGNode : public DFGNodeBase {
		public:
			friend CGRADFG;
			enum class NodeKind {
				Compute,
				MemLoad,
				MemStore,
				Compare,
				Constant,
				VirtualRoot,
			};

			DFGNode(int ID, NodeKind kind, Value *val) :
				DFGNodeBase(), ID(ID), kind(kind), val(val) {};

			DFGNode(NodeKind kind, Value* val) :
				DFGNode((std::uintptr_t)(val), kind, val) {}

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

			NodeKind getKind() const {
				return kind;
			}

			int getID() const { return ID; }
			Value* getValue() const { return val; }

			virtual string getUniqueName() const = 0;
			virtual string getNodeAttr() const = 0;
			virtual string getExtraInfo() const { return ""; };

			bool isEqualTo(const DFGNode &N) const {
				return this->ID == N.ID;
			}

		protected:
			NodeKind kind;
			int ID;
			Value *val;
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
			string getUniqueName() const {
				return "__VROOT";
			}
			string getNodeAttr() const {
				return "";
			}
	};

	/**
	 * @class ComputeNode
	 * @brief A concrete class for computational nodes
	*/
	class ComputeNode : public DFGNode {
		public:
			ComputeNode(Instruction* inst, std::string opcode) : 
				DFGNode(DFGNode::NodeKind::Compute, inst), opcode(opcode) {}

			string getUniqueName() const {
				return opcode + "_" + to_string(getID());
			}
			string getNodeAttr() const {
				return formatv("type=op,{0}={1}", OptDFGOpKey, opcode);
			}
			static bool classof(const DFGNode* N) {
				return N->getKind() == NodeKind::Compute;
			}
			Instruction* getInst() const {
				return dyn_cast<Instruction>(val);
			}
		private:
			std::string opcode;
	};

	template<DFGNode::NodeKind KIND>
	class MemAccessNode : public DFGNode {
		public:
			MemAccessNode(Value *addr) : 
					DFGNode(KIND, addr) {
				_isLoad = KIND == DFGNode::NodeKind::MemLoad;
			}

			inline bool isLoad() { return _isLoad;}

			string getUniqueName() const {
				switch (KIND) {
					case DFGNode::NodeKind::MemLoad:
						return  "Load_" + to_string(getID());
					case DFGNode::NodeKind::MemStore:
						return  "Store_" + to_string(getID());
					default:
						assert("Unexprected node kind for memory access node");
						return "";
				}
			}
			string getNodeAttr() const {
				string type;
				switch (KIND) {
					case DFGNode::NodeKind::MemLoad:
						type = "input";
						break;
					case DFGNode::NodeKind::MemStore:
						type = "output";
						break;
					default:
						assert("Unexprected node kind for memory access node");
						return "";
				}
				return formatv("type={0},data={1}", type, getSymbol());
			}

			string getSymbol() const {
				if (auto gep = dyn_cast<GetElementPtrInst>(val)) {
					auto *ptr = gep->getPointerOperand();
					return string(ptr->getName());
				} else {
					return "unknown";
				}
			}

			static bool classof(const DFGNode* N) {
				return N->getKind() == NodeKind::MemLoad ||
						N->getKind() == NodeKind::MemStore;
			}
		private:
			bool _isLoad;
	};

	class ConstantNode : public DFGNode {
		public:
			ConstantNode(Constant *c) : 
				DFGNode(DFGNode::NodeKind::Constant, c) {}
			string getUniqueName() const {
				return "Const_" + to_string(getID());
			}
			string getNodeAttr() const {
				return formatv("type=const,{0}", getConstStr());
			}
			string getExtraInfo() const {
				return getConstStr();
			}
			static bool classof(const DFGNode *N) {
				return N->getKind() == NodeKind::Constant;
			}
		private:
			Constant *const_value;
			string getConstStr() const;

			string getFloatType(APFloat f) const {
				switch (APFloatBase::SemanticsToEnum(f.getSemantics())) {
					case APFloat::Semantics::S_IEEEhalf:
						return "float16";
					case APFloat::Semantics::S_IEEEsingle:
						return "float32";
					case APFloat::Semantics::S_IEEEdouble:
						return "float64";
					case APFloat::Semantics::S_IEEEquad:
						return "float128";
					default:
						return "unknown";
				}
			}
	};


	/**
	 * @class DFGEdge
	 * @brief Class of DFG edge derived from DGEdge
	*/
	class DFGEdge : public DFGEdgeBase {
		public:
			DFGEdge(DFGNode &N, int operand = 0) : DFGEdgeBase(N), operand(operand) {}
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

			string getEdgeAttr() const {
				return formatv("operand={0}", operand);
			}
		private:
			int operand;
	};

	/**
	 * @class CGRADFG
	 * @brief A graph class for CGRA kernel DFG derived from @em llvm::DirectedGraph
	 * @see https://llvm.org/doxygen/classllvm_1_1DirectedGraph.html
	 * @details To handle this graph class by LLVM utilities (such as traversal), it needs an entry node. However, data-flow-graphs have more than one nodes which has no in-coming edge. Therefore, this class has a virtual root node, which is connected to those node and does not correspond to any LLVM IR values. When exporting this graph instance as DOT file, the virtual root and its edges are eliminated.
	 */
	class CGRADFG : public CGRADFGBase {
		public:
			using NodeType = DFGNode;
			using EdgeType = DFGEdge;
			using EdgeInfoType = std::pair<NodeType*, EdgeListTy>;

			/// default constructor
			CGRADFG() : CGRADFGBase() {
				createVirtualRoot();
			};
			CGRADFG(const CGRADFG &G) = delete;
			/// move constructor
			CGRADFG(CGRADFG &&G) : CGRADFGBase(std::move(G)) {
				virtual_root = G.virtual_root;
				G.virtual_root = nullptr;
			};

			/// constructor with an initial node
			CGRADFG(NodeType &N) : CGRADFGBase(N) {
				createVirtualRoot();
				auto E = new DFGEdge(N);
				connect(getRoot(), N, *E);
			};

			/// Destructor
			~CGRADFG() {
				delete virtual_root;
				Nodes.clear();
			}

			/**
			 * @brief Get the virtual route node object
			 * 
			 * @return NodeType&: a reference to the virtual root
			 */
			NodeType &getRoot() const {
				return *virtual_root;
			}

			/**
			 * @brief  add a new node to the graph
			 *
			 * @param N a DFG node to be added
			 * @return NodeType* actually added node
			 * If an node same as the passed node N is already added,
			 * it returns the pointer of that node.
			 * In case of error, it returns nullptr;
			 */
			NodeType* addNode(NodeType &N);

			/**
			 * @brief connect two nodes with an edge
			 * 
			 * @param Src source node
			 * @param Dst destination node
			 * @param E an edge pointing to the dest. node
			 * @return true in the case of no error
			 * @return Otherwise, false
			 */
			bool connect(NodeType &Src, NodeType &Dst, EdgeType &E);

			/**
			 * @brief find in-coming edges and get the list of them
			 * Unlike the same name method in @em llvm::DirectedGraph,
			 * this keeps source nodes of the egdges.
			 * If you want to ignore the virtual root, set ignore vroot to be true
			 * 
			 * @param N Node
			 * @param EL a list of edge infomation (edges + src node)
			 * @param ignore_vroot whether the virtual root is ignored or not (Default: false)
			 * @return it returns true if any edges are found
			 * @return Otherwise it returns false
			 */
			bool findIncomingEdgesToNode(const NodeType &N,
							SmallVectorImpl<EdgeInfoType> &EL,
							bool ignore_vroot = false) const {
				assert(EL.empty() && "Expected the list of edges to be empty.");
				EdgeListTy TempList;
				for (auto *Node : Nodes) {
					if (*Node == N)
						continue;
					if (ignore_vroot && *Node == getRoot()) continue;
					if (Node->findEdgesTo(N, TempList)) {
						EL.push_back(std::make_pair(Node,TempList));
					}
					TempList.clear();
				}
				return !EL.empty();
			}

			/**
			 * @brief convert "Node_" + @a pointer style node name to more plain name
			 * 
			 * @param dot_string contents of the DOT file
			 * @return string converted contents
			 */
			string convertToReadableNodeName(const string dot_string) const;

			/**
			 * @brief save the graph as DOT file
			 * 
			 * @param filepath filepath of the save file
			 * @return Error in the case of failure in creating a new file (e.g., because the same name file already exists)
			 */
			Error saveAsDotGraph(StringRef filepath);

			/**
			 * @brief Set the Name object
			 * 
			 * @param graph_name name of the graph
			 */
			void setName(const string graph_name) {
				name = graph_name;
			}

			/**
			 * @brief Get the Name object
			 * 
			 * @return string of the graph name
			 */
			string getName() const {
				return name;
			}

			void makeSequentialNodeID();

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
				return Node->getUniqueName();
			}

			static string getNodeIdentifierLabel(const DFGNode *Node, 
								const CGRADFG *G) {
				return Node->getExtraInfo();
			}

			static string getNodeDescription(const DFGNode *Node, 
								const CGRADFG *G) {
				return "";
			}

			static string getNodeAttributes(const DFGNode *Node, 
								const CGRADFG *G) {
				return Node->getNodeAttr();
			}

			static string getEdgeAttributes(const DFGNode *Node, 
					GraphTraits<DFGNode *>::ChildIteratorType I,
					  const CGRADFG *G);
		private:
			/**
			 * @brief  a default graph properties for DOT graph
			**/
			static StringMap<StringRef> default_graph_prop;
			/**
			 * @brief  a default node propterties for DOT graph
			**/
			static StringMap<StringRef> default_node_prop;
			/**
			 * @brief  a default edge propterties for DOT graph
			**/
			static StringMap<StringRef> default_edge_prop;

	};

	using CGRADFGDotGraphTraits = DOTGraphTraits<const CGRADFG *>;

};

#endif //CGRADataFlowGraph_H