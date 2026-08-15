import { PUBLIC_API_URL } from '$env/static/public';
import type { PageServerLoad } from './$types';

export const load: PageServerLoad = async () => {
	const res = await fetch(`${PUBLIC_API_URL}/api/issue?page=1&limit=3&status=PUBLISHED`, {
		method: 'GET'
	});

	if (!res.ok) {
		return {
			message: 'Something went wrong',
			status: res.status
		};
	}

	const data = await res.json();

	return data;
};
