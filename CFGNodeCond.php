<?php
include_once "CFGNode.php";

// Class that represents a conditional CFG node such as an if conditional and 
// loop header.
class CFGNodeCond extends CFGNode {

// Constant definition for class of loops.
const FOREACH_LOOP=1;
const WHILE_LOOP=2;
const FOR_LOOP=3;

// The conditional expression in the node.
public $expr = NULL;

// Boolean that represents whether this node is a loop header.
public $is_loop_header=FALSE;

// Value that represents the type of loop header. 
// When defined, it's a class constant.
public $loop_type=NULL;

public function __construct() {

       parent::__construct();	      	     

       $expr = NULL;
       $is_loop_header=FALSE;
       $loop_type=NULL;
}

// The true successor of a conditional node is the first successor.
public function getTrueSuccessor() {

       return $this->successors[0];
}

// The false successor of a conditional node is the second successor.
public function getFalseSuccessor() {

       return $this->successors[1];
}
	
}
?>