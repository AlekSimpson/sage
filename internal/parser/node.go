package sage

import (
	"fmt"
	"sage/internal/lexer"
)

type NodeType int

const (
	BINARY NodeType = iota
	TRINARY
	UNARY
	NUMBER
	STRING
	IDENTIFIER
	KEYWORD
	BLOCK
	CODE_BLOCK
	PARAM_LIST
	FUNCDEF
	FUNCCALL
	TYPE
	STRUCT
	IF
	IF_BRANCH
	ELSE_BRANCH
	WHILE
	ASSIGN
	FOR
	PROGRAM
	RANGE
	VAR_DEC
	VAR_REF
	COMPILE_TIME_EXECUTE
)

func (nt NodeType) String() string {
	switch nt {
	case BINARY:
		return "BINARY"
	case TRINARY:
		return "TRINARY"
	case UNARY:
		return "UNARY"
	case NUMBER:
		return "NUMBER"
	case IDENTIFIER:
		return "IDENTIFIER"
	case KEYWORD:
		return "KEYWORD"
	case BLOCK:
		return "BLOCK"
	case CODE_BLOCK:
		return "CODE_BLOCK"
	case PARAM_LIST:
		return "PARAM_LIST"
	case FUNCDEF:
		return "FUNCDEF"
	case FUNCCALL:
		return "FUNCCALL"
	case TYPE:
		return "TYPE"
	case STRUCT:
		return "STRUCT"
	case IF:
		return "IF"
	case IF_BRANCH:
		return "IF_BRANCH"
	case ELSE_BRANCH:
		return "ELSE_BRANCH"
	case WHILE:
		return "WHILE"
	case ASSIGN:
		return "ASSIGN"
	case FOR:
		return "FOR"
	case PROGRAM:
		return "PROGRAM"
	case RANGE:
		return "RANGE"
	case VAR_DEC:
		return "VAR_DEC"
	case VAR_REF:
		return "VAR_REF"
	case STRING:
		return "STRING"
	case COMPILE_TIME_EXECUTE:
		return "COMPILE_TIME_EXECUTE"
	default:
		return "Unknown Node Type (Could have forgot to add String() impl for new type)"
	}
}

type ParseNode interface {
	String() string
	Showtree(depth string)
	get_token() *sage.Token
	get_nodetype() NodeType
}

//// BEGIN BLOCK NODE

type BlockNode struct {
	token    *sage.Token
	nodetype NodeType
	children []ParseNode
}

func ModuleRootNode() *BlockNode {
	rootTok := &sage.Token{
		Lexeme:  "",
		Linenum: 0,
	}

	return &BlockNode{
		token:    rootTok,
		nodetype: PROGRAM,
	}
}

func NewBlockNode(tok *sage.Token, c []ParseNode) *BlockNode {
	return &BlockNode{
		token:    tok,
		children: c,
	}
}

func (n *BlockNode) get_nodetype() NodeType {
	return BLOCK
}

func (n *BlockNode) get_token() *sage.Token {
	return n.token
}

func (n *BlockNode) String() string {
	return "BLOCK_NODE"
}

func (n *BlockNode) Showtree(depth string) {
	fmt.Println(depth + "- " + n.String())

	for _, child := range n.children {
		child.Showtree(depth + "\t")
	}
}

//// BEGIN BINARY NODE

type BinaryNode struct {
	token    *sage.Token
	nodetype NodeType
	left     ParseNode
	right    ParseNode
}

func NewBinaryNode(t *sage.Token, nt NodeType, l ParseNode, r ParseNode) *BinaryNode {
	return &BinaryNode{
		token:    t,
		nodetype: nt,
		left:     l,
		right:    r,
	}
}

func (n *BinaryNode) get_nodetype() NodeType {
	return BINARY
}

func (n *BinaryNode) get_token() *sage.Token {
	return n.token
}

func (n *BinaryNode) String() string {
	return fmt.Sprintf("BINARY NODE (%s: %s | %s | %s)", n.nodetype, n.token.Lexeme, n.left.get_token().Lexeme, n.right.get_token().Lexeme)
}

func (n *BinaryNode) Showtree(depth string) {
	fmt.Println(depth + "- " + n.String())

	if n.left != nil {
		n.left.Showtree(depth + "\t")
	}
	if n.right != nil {
		n.right.Showtree(depth + "\t")
	}
}

//// BEGIN TRINARY NODE

type TrinaryNode struct {
	token    *sage.Token
	nodetype NodeType
	left     ParseNode
	middle   ParseNode
	right    ParseNode
}

func NewTrinaryNode(t *sage.Token, nodetype NodeType, left ParseNode, middle ParseNode, right ParseNode) *TrinaryNode {
	return &TrinaryNode{
		token:    t,
		nodetype: nodetype,
		left:     left,
		middle:   middle,
		right:    right,
	}
}

func (n *TrinaryNode) get_nodetype() NodeType {
	return TRINARY
}

func (n *TrinaryNode) get_token() *sage.Token {
	return n.token
}

func (n *TrinaryNode) String() string {
	return fmt.Sprintf("TRINARY NODE (%s: %s | %s | %s | %s)", n.nodetype, n.token.Lexeme, n.left.get_token().Lexeme, n.middle.get_token().Lexeme, n.right.get_token().Lexeme)
}

func (n *TrinaryNode) Showtree(depth string) {
	fmt.Println(depth + "- " + n.String())

	if n.left != nil {
		n.left.Showtree(depth + "\t")
	}
	if n.middle != nil {
		n.middle.Showtree(depth + "\t")
	}
	if n.right != nil {
		n.right.Showtree(depth + "\t")
	}
}

//// BEGIN UNARY NODE

type UnaryNode struct {
	token    *sage.Token
	nodetype NodeType
	node     ParseNode
	tag      string // author's message indicating extra meta information about node
}

func (n *UnaryNode) get_nodetype() NodeType {
	return UNARY
}

func (n *UnaryNode) String() string {
	if n.node != nil {
		return fmt.Sprintf("UNARY NODE (%s: %s | %s)", n.nodetype, n.token.Lexeme, n.node)
	}

	return fmt.Sprintf("UNARY NODE (%s: %s)", n.nodetype, n.token.Lexeme)
}

func (n *UnaryNode) get_token() *sage.Token {
	return n.token
}

func (n *UnaryNode) Showtree(depth string) {
	fmt.Println(depth + "- " + n.String())

	if n.node != nil {
		n.node.Showtree(depth + "\t")
	}
}

func (n *UnaryNode) addtag(message string) {
	n.tag = message
}

func NewUnaryNode(tok *sage.Token, nodetype NodeType) *UnaryNode {
	return &UnaryNode{
		token:    tok,
		nodetype: nodetype,
		tag:      "",
	}
}

func NewBranchUnaryNode(tok *sage.Token, nodetype NodeType, branch_node ParseNode) *UnaryNode {
	return &UnaryNode{
		token:    tok,
		nodetype: nodetype,
		node:     branch_node,
		tag:      "",
	}
}
