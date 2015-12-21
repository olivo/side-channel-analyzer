<?php

class CFGNode {

// Constant definition for class of loops.
const FOREACH_LOOP=1;
const WHILE_LOOP=2;
const FOR_LOOP=3;

// The statement contained in the node.
public $stmt = NULL;

// The set of pointers to successor CFG nodes.
// On conditional nodes, the first successor is the true branch,
// and the second successor is the false branch.
public $successors = array();

// Boolean that represents whether this node is a conditional node.
public $is_cond=FALSE;

// Boolean that represents whether this node is a loop header.
public $is_loop_header=FALSE;

// Value that represents the type of loop header. 
// When defined, it's a class constant.
public $loop_type=NULL;

// Boolean that represents whether the successor of the current node 
// is traversed by a back_edge.
public $has_backedge = FALSE;

	
public function __construct() {
	      	     
	$this->stmt = NULL;
	
	$this->successors = array();

	$this->is_cond = FALSE;

	$this->is_loop_header = FALSE;

	$this->loop_type = NULL;

	$this->has_backedge = FALSE;
 }
	
 }



?>