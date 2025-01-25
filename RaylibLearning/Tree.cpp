#include <iostream>
#include <list>
#include "raymath.h"


// Tree Node class
class TreeNode {
public:
    TreeNode(Vector3 value) : data(value) {}

    void addChild(std::shared_ptr<TreeNode>& child) {
        children.push_back(child);
    }

    const std::list<std::shared_ptr<TreeNode>>& getChildren() {
        return children;
    }

    const Vector3& getData() {
        return data;
    }
    // Made the findNode function public
    /*TreeNode* findNode(int value) {
        return findNodeHelper(this, value);
    }*/

private:
    Vector3 data;
    std::list<std::shared_ptr<TreeNode>> children;
    
    // Renamed the findNode function to findNodeHelper to make it private
    /*TreeNode* findNodeHelper(TreeNode* node, Point value) {
        if (node->data == value) {
            return node;
        }

        for (TreeNode* child : node->children) {
            TreeNode* found = findNodeHelper(child, value);
            if (found != nullptr) {
                return found;
            }
        }

        return nullptr;
    }*/
};

// Tree class
class Tree {
public:
    TreeNode* root;


    Tree(Vector3 rootValue) {
        root = new TreeNode(rootValue);
    }

    // Recursive function to print the tree
    void printTree(const std::shared_ptr<TreeNode>& node) {
        if (!node) return;
        std::cout << "(" << node->getData().x << ", " << node->getData().y << ")";
        if (!node->getChildren().empty()) {
            std::cout << " -> ";
            printTree(node->getChildren().front()); // For simplicity, take the first path (DFS order)
        }
    }



    // Calling the public findNode function of TreeNode
    /*TreeNode* findNode(int value) {
        return root->findNode(value);
    }*/
};