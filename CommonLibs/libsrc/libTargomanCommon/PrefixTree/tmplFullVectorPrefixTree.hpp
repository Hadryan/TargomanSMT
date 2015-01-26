#ifndef GFULLVECTOR_PREFIX_TREE_HH
#define GFULLVECTOR_PREFIX_TREE_HH

#include "libTargomanCommon/PrefixTree/tmplAbstractPrefixTree.hpp"
#include "libTargomanCommon/PrefixTree/tmplVectorPrefixTree.hpp"

namespace Targoman{
namespace Common{
namespace PrefixTree {
    template <typename clsData_t> class tmplPrefixTreeFullVectorNode : virtual public tmplPrefixTreeAbstractNode<unsigned, clsData_t> {
	protected:
        typedef std::vector<tmplPrefixTreeVectorNode<clsData_t> *> Container_;
        typedef tmplPrefixTreeVectorNode<clsData_t> *& ContainerElementReference_;
        typedef typename Container_::iterator ContainerIterator_;
        typedef typename Container_::const_iterator ContainerConstIterator_;
        typedef tmplPrefixTreeAbstractNode<unsigned, clsData_t> AbstractNode_;
        typedef typename AbstractNode_::AbstractWeakIterator AbstractWeakIterator_;
        typedef tmplPrefixTreeVectorNode<clsData_t> VectorNode_;
    public:
        tmplPrefixTreeFullVectorNode(unsigned index, AbstractNode_ *predecessor) : AbstractNode_(index, predecessor) {}
        tmplPrefixTreeFullVectorNode(unsigned index, AbstractNode_ *predecessor, const clsData_t &data) : AbstractNode_(index, predecessor, data) {}
        ~tmplPrefixTreeFullVectorNode() {
            for (ContainerIterator_ i = successors_.begin(); i != successors_.end(); ++i)
                delete *i;
        }

        AbstractNode_ *follow(const unsigned &nextIndex) const {
            AbstractNode_ *returnPointer;
            if (successors_.size() <= nextIndex)
                returnPointer = 0;
            else
                returnPointer = successors_[nextIndex];
            return returnPointer;
        }

        AbstractNode_ *followOrExpand(const unsigned &nextIndex) {
            if (successors_.size() <= nextIndex)
                successors_.resize(nextIndex+1, 0);
            if (!successors_[nextIndex])
                successors_[nextIndex] = new tmplPrefixTreeVectorNode<clsData_t>(nextIndex, this); // Note: this is NOT a fullVectorNode
            return successors_[nextIndex];
        }

        AbstractNode_ *followOrExpand(const unsigned &nextIndex, const clsData_t &standardValue) {
            if (successors_.size() <= nextIndex)
                successors_.resize(nextIndex+1, 0);
            if (!successors_[nextIndex])
                successors_[nextIndex] = new tmplPrefixTreeVectorNode<clsData_t>(nextIndex, this, standardValue); // Note: this is NOT a fullVectorNode
            return successors_[nextIndex];
        }

        /**
         * It is assumed that the vector is only initialized "on demand". This
         * is true in the current implementation but it could change!! In this
         * case we probably would have to do a traversal of the vector in order
         * to check for succesors.
         */
        virtual bool isLeaf() const {
            return successors_.empty();
        }

        virtual void compact() {
            tmplShrinkToFitVector(successors_);
        }

        class WeakIterator : public AbstractWeakIterator_ {
        public:
            WeakIterator(const ContainerIterator_ &successorsIterator, const ContainerIterator_ &endIterator) :
                AbstractWeakIterator_(), successorsIterator_(successorsIterator), endIterator_(endIterator)
            {
                while (successorsIterator_ != endIterator_ && !*successorsIterator_)
                    ++successorsIterator_;
            }

            AbstractWeakIterator_ *increase() { 
                do {
                    ++successorsIterator_; 
                } while (successorsIterator_ != endIterator_ && !*successorsIterator_);
                return this;
            }
            AbstractWeakIterator_ *decrease() { --successorsIterator_; return this; }

            AbstractNode_ *dereference() const { return *successorsIterator_; }

            virtual bool equals(const AbstractWeakIterator_ *otherWeakIterator) const {
                const WeakIterator *castedIterator = static_cast<const WeakIterator *>(otherWeakIterator);
                return castedIterator->successorsIterator_ == successorsIterator_;
            }

            AbstractWeakIterator_ *getCopy() const {
                return new WeakIterator(successorsIterator_, endIterator_);
            }

        private:
            ContainerIterator_ successorsIterator_;
            ContainerIterator_ endIterator_;
        };

        AbstractWeakIterator_ *weakBegin() { return new WeakIterator(successors_.begin(), successors_.end()); }
        AbstractWeakIterator_ *weakEnd() { return new WeakIterator(successors_.end(), successors_.end()); }
		
		bool insert(const unsigned &index, AbstractNode_ *node)
		{
			// increase the size
			if(index >= successors_.size())
				successors_.resize(index + 1, 0);
			
			// test if a successor with this index already exists
			if(successors_[index] != 0)
				return false;
			
			successors_[index] = dynamic_cast<VectorNode_*>(node);
			return true;
		}
		
		void erase(const unsigned &index) 
		{
			successors_[index] = 0;
		}
		
		void clear()
		{
			for (ContainerIterator_ i = successors_.begin(); i != successors_.end(); ++i)
				delete *i;
			successors_.clear();
		}

    protected:
        mutable Container_ successors_;
    };

    //! A Prefix Tree where the container is a full-sized vector
    /**
     * The data is stored in a (sorted) vector, resized to allow direct access
     * to all the elements stores. Indexes are limited to unsigned. Retrieval
     * is very efficient, insertion depends on how often the vector has to be
     * resized. Storage efficiency is of course horrible for sparse nodes.
     *
     * Template equivalent: <tt>GAbstractPrefixTree<tmplPrefixTreeVectorNode<DataClass> ></tt>
     * \sa GAbstractPrefixTree
     *
     * \todo specify initial size
     */
    template <class DataClass> class tmplFullVectorPrefixTree : public tmplAbstractPrefixTree<tmplPrefixTreeFullVectorNode<DataClass> > {};
}
}
}
#endif
