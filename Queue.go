package main

type Queueable interface {
	string | int | byte | Token
}

type Queue[Q Queueable] struct {
	array     []Q
	nullValue Q
}

func NewQueue[Q Queueable](arr []Q, nullvalue Q) *Queue[Q] {
	return &Queue[Q]{
		array:     arr,
		nullValue: nullvalue,
	}
}

func (q *Queue[Q]) push(value Q) {
	q.array = append(q.array, value)
}

func (q *Queue[Q]) pop() Q {
	if len(q.array) == 0 {
		return q.nullValue
	} else if len(q.array) == 1 {
		retval := q.array[0]
		q.array = make([]Q, 0)
		return retval
	}

	retval := q.array[0]
	q.array = q.array[1:]
	return retval
}

func (q *Queue[Q]) empty() bool {
	return len(q.array) == 0
}
