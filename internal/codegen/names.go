package sage

import (
	xid "github.com/rs/xid"
)

func CreateInternalName() string {
	return xid.New().String()
}
