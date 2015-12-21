<?php
include_once "CFGNode.php";

// Class that corresponds to an individual statement CFG node (assignment, method calls, etc.).

class CFGNodeStmt extends CFGNode {

// The statement contained in the node.
public $stmt = NULL;

// Boolean that represents whether the successor of the current node 
// is traversed by a back_edge.
public $has_backedge = FALSE;


public function __construct() {

       parent::__construct();

       $this->stmt = NULL;
       $this->has_backedge = FALSE;
}
	
}
?>