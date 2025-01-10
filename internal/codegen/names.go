package sage

import (
	xid "github.com/rs/xid"
)

func CreateInternalLocalName() string {
	return "%" + xid.New().String()
}

func CreateInternalGlobalName() string {
	return "@" + xid.New().String()
}
