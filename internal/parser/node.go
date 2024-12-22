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
	LIST
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
	case LIST:
		return "LIST"
	default:
		return "Unknown Node Type (Could have forgot to add String() impl for new type)"
	}
}

type ParseNode interface {
	String() string
	Showtree(depth string)
	Get_token() *sage.Token
	Get_nodetype() NodeType // identifies the host structure type (Unary vs Block vs List vs Binary vs Trinary)
	Get_child_node() ParseNode
	Get_true_nodetype() NodeType // identifies the set nodetype
}

//// BEGIN BLOCK NODE

type BlockNode struct {
	token    *sage.Token
	Nodetype NodeType
	Children []ParseNode
}

func ModuleRootNode() *BlockNode {
	rootTok := &sage.Token{
		Lexeme:  "",
		Linenum: 0,
	}

	return &BlockNode{
		token:    rootTok,
		Nodetype: PROGRAM,
	}
}

func NewBlockNode(tok *sage.Token, c []ParseNode) *BlockNode {
	return &BlockNode{
		token:    tok,
		Children: c,
	}
}

func (n *BlockNode) Get_true_nodetype() NodeType {
	return n.Nodetype
}

func (n *BlockNode) Get_child_node() ParseNode {
	if len(n.Children) == 0 {
		return nil
	}
	return n.Children[0]
}

func (n *BlockNode) Get_nodetype() NodeType {
	return BLOCK
}

func (n *BlockNode) Get_token() *sage.Token {
	return n.token
}

func (n *BlockNode) String() string {
	return "BLOCK_NODE"
}

func (n *BlockNode) Showtree(depth string) {
	fmt.Println(depth + "- " + n.String())

	for _, child := range n.Children {
		child.Showtree(depth + "\t")
	}
}

//// BEGIN BINARY NODE

type BinaryNode struct {
	token    *sage.Token
	Nodetype NodeType
	Left     ParseNode
	Right    ParseNode
}

func NewBinaryNode(t *sage.Token, nt NodeType, l ParseNode, r ParseNode) *BinaryNode {
	return &BinaryNode{
		token:    t,
		Nodetype: nt,
		Left:     l,
		Right:    r,
	}
}

func (n *BinaryNode) Get_true_nodetype() NodeType {
	return n.Nodetype
}

func (n *BinaryNode) Get_child_node() ParseNode {
	return n.Left
}

func (n *BinaryNode) Get_nodetype() NodeType {
	return BINARY
}

func (n *BinaryNode) Get_token() *sage.Token {
	return n.token
}

func (n *BinaryNode) String() string {
	return fmt.Sprintf("BINARY NODE (%s: %s | %s | %s)", n.Nodetype, n.token.Lexeme, n.Left.Get_token().Lexeme, n.Right.Get_token().Lexeme)
}

func (n *BinaryNode) Showtree(depth string) {
	fmt.Println(depth + "- " + n.String())

	if n.Left != nil {
		n.Left.Showtree(depth + "\t")
	}
	if n.Right != nil {
		n.Right.Showtree(depth + "\t")
	}
}

//// BEGIN TRINARY NODE

type TrinaryNode struct {
	token    *sage.Token
	Nodetype NodeType
	left     ParseNode
	middle   ParseNode
	right    ParseNode
}

func NewTrinaryNode(t *sage.Token, nodetype NodeType, left ParseNode, middle ParseNode, right ParseNode) *TrinaryNode {
	return &TrinaryNode{
		token:    t,
		Nodetype: nodetype,
		left:     left,
		middle:   middle,
		right:    right,
	}
}

func (n *TrinaryNode) Get_true_nodetype() NodeType {
	return n.Nodetype
}

func (n *TrinaryNode) Get_child_node() ParseNode {
	return n.left
}

func (n *TrinaryNode) Get_nodetype() NodeType {
	return TRINARY
}

func (n *TrinaryNode) Get_token() *sage.Token {
	return n.token
}

func (n *TrinaryNode) String() string {
	return fmt.Sprintf("TRINARY NODE (%s: %s | %s | %s | %s)", n.Nodetype, n.token.Lexeme, n.left.Get_token().Lexeme, n.middle.Get_token().Lexeme, n.right.Get_token().Lexeme)
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
	Nodetype NodeType
	node     ParseNode
	Tag      string // author's message indicating extra meta information about node
}

func (n *UnaryNode) Get_true_nodetype() NodeType {
	return n.Nodetype
}

func (n *UnaryNode) Get_child_node() ParseNode {
	return n.node
}

func (n *UnaryNode) Get_nodetype() NodeType {
	return UNARY
}

func (n *UnaryNode) String() string {
	if n.node != nil {
		return fmt.Sprintf("UNARY NODE (%s: %s | %s)", n.Nodetype, n.token.Lexeme, n.node)
	}

	return fmt.Sprintf("UNARY NODE (%s: %s)", n.Nodetype, n.token.Lexeme)
}

func (n *UnaryNode) Get_token() *sage.Token {
	return n.token
}

func (n *UnaryNode) Showtree(depth string) {
	fmt.Println(depth + "- " + n.String())

	if n.node != nil {
		n.node.Showtree(depth + "\t")
	}
}

func (n *UnaryNode) addtag(message string) {
	n.Tag = message
}

func NewUnaryNode(tok *sage.Token, nodetype NodeType) *UnaryNode {
	return &UnaryNode{
		token:    tok,
		Nodetype: nodetype,
		Tag:      "",
	}
}

func NewBranchUnaryNode(tok *sage.Token, nodetype NodeType, branch_node ParseNode) *UnaryNode {
	return &UnaryNode{
		token:    tok,
		Nodetype: nodetype,
		node:     branch_node,
		Tag:      "",
	}
}

//// BEGIN LISTNODE

type ListNode struct {
	token    *sage.Token
	Nodetype NodeType
	Lexemes  []string
	node     ParseNode
}

func NewListNode(token *sage.Token, nodetype NodeType, lexemes []string) *ListNode {
	return &ListNode{
		token:    token,
		Nodetype: nodetype,
		Lexemes:  lexemes,
		node:     nil,
	}
}

func (n *ListNode) Get_true_nodetype() NodeType {
	return n.Nodetype
}

func (n *ListNode) Get_child_node() ParseNode {
	return n.node
}

func (n *ListNode) Get_nodetype() NodeType {
	return LIST
}

func (n *ListNode) String() string {
	if n.node != nil {
		return fmt.Sprintf("LIST NODE (%s: %s | %s)", n.Nodetype, n.Get_full_lexeme(), n.node)
	}

	return fmt.Sprintf("LIST NODE (%s: %s)", n.Nodetype, n.Get_full_lexeme())
}

func (n *ListNode) Get_full_lexeme() string {
	full_lexeme := ""
	for _, lex := range n.Lexemes {
		full_lexeme += lex
		full_lexeme += "."
	}
	full_lexeme = full_lexeme[:len(full_lexeme)-1]

	return full_lexeme
}

func (n *ListNode) Get_token() *sage.Token {
	return n.token
}

func (n *ListNode) Showtree(depth string) {
	fmt.Println(depth + "- " + n.String())

	if n.node != nil {
		n.node.Showtree(depth + "\t")
	}
}
