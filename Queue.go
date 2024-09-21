package main

type Queueable interface {
    string | int | byte | Token
}

type Queue[Q Queueable] struct {
    array []Q
}

func NewQueue[Q Queueable](arr []Q) *Queue[Q] {
    return &Queue[Q]{
        array: arr,
    }
}

func (q *Queue[Q]) push(value Q) {
    q.array = append(q.array, value)
}

func (q *Queue[Q]) pop() Q {
    retval := q.array[0]
    q.array = q.array[1:]
    return retval
}

func (q *Queue[Q]) empty() bool {
    return len(q.array) == 0
}

